#include "Gerber_Parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <cmath>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Gerber_Parser::ParserState::reset() {
    currentPoint = QPointF();
    units = GerberUnit::Millimeters;
    apertures.clear();
    currentAperture = -1;
    polarity = Polarity::Dark;
    interpolation = InterpolationMode::Linear;
    inRegion = false;
    regionPoints.clear();
    absoluteMode = true;
    intDigits = 2;
    fracDigits = 4;
}

Gerber_Parser::Gerber_Parser(QObject* parent) : QObject(parent) {}

void Gerber_Parser::clear() {
    m_lastError.clear();
    m_state.reset();
    m_layer = GerberLayer();
}

GerberLayer Gerber_Parser::parseFile(const QString& filePath, const GerberParseOptions& opts) {
    clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot open file: " + filePath;
        m_layer.errorMessage = m_lastError;
        return m_layer;
    }

    QFileInfo fi(filePath);
    m_layer.fileName = fi.fileName();
    QTextStream in(&file);
    // ====== ТЕСТ ЧТЕНИЯ ======
    QString firstLine = in.readLine();
    ///qDebug() << "=== FIRST LINE OF FILE:" << firstLine;
    in.seek(0);

    int lineNum = 0, totalLines = 0;
    QTextStream cnt(&file); cnt.seek(0);
    while (!cnt.atEnd()) { cnt.readLine(); totalLines++; }
    file.seek(0); in.seek(0);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNum++;

        if (totalLines > 0 && lineNum % 100 == 0)
            emit parseProgress(qMin(lineNum * 100 / totalLines, 99));

        int ci = line.indexOf("G04");
        if (ci >= 0) line = line.left(ci).trimmed();
        if (line.isEmpty()) continue;

        if (opts.debugOutput)
            m_layer.commands.append({ line, lineNum });

        processLine(line, lineNum);
    }

    file.close();

    // Вычисляем boundingRect
    for (auto& p : m_layer.polygons)
        m_layer.boundingRect |= p.boundingRect();
    for (auto& s : m_layer.segments)
        m_layer.boundingRect |= QRectF(s.startPoint, s.endPoint);

    m_layer.valid = m_lastError.isEmpty() || !m_layer.polygons.isEmpty() || !m_layer.segments.isEmpty();
    if (m_layer.valid) m_layer.units = m_state.units;

    qDebug() << "Parse result:" << m_layer.fileName
        << "polygons:" << m_layer.polygons.size()
        << "segments:" << m_layer.segments.size();

    emit parseProgress(100);
    // Нормализация координат: делаем их относительными
    if (!m_layer.boundingRect.isEmpty()) {
        QPointF topLeft = m_layer.boundingRect.topLeft();
        qDebug() << "Normalizing coordinates. Offset:" << topLeft;
        qDebug() << "Original boundingRect:" << m_layer.boundingRect;

        // Сдвигаем все полигоны
        for (auto& poly : m_layer.polygons) {
            poly.translate(-topLeft.x(), -topLeft.y());
        }

        // Сдвигаем все сегменты
        for (auto& seg : m_layer.segments) {
            seg.startPoint -= topLeft;
            seg.endPoint -= topLeft;
            seg.centerPoint -= topLeft;
        }

        // Обновляем boundingRect (теперь будет от 0,0)
        m_layer.boundingRect.translate(-topLeft.x(), -topLeft.y());

        qDebug() << "Normalized boundingRect:" << m_layer.boundingRect;
    }
    // Небольшой отступ по краям
    m_layer.boundingRect.adjust(-2, -2, 2, 2);
    qDebug() << "BoundingRect after normalize:" << m_layer.boundingRect;
    qDebug() << "Width:" << m_layer.boundingRect.width() << "Height:" << m_layer.boundingRect.height();

    return m_layer;
}

bool Gerber_Parser::processLine(const QString& line, int lineNum)
{
    Q_UNUSED(lineNum)
#if 0
        // ОТЛАДКА — показать каждую строку с D03
        if (line.contains("D03")) {
            qDebug() << "!! LINE WITH D03:" << line;
        }
    if (line.contains('X')) {
        qDebug() << "!! LINE WITH X:" << line;
    }
#endif
        // Команды в %...%
        if (line.startsWith('%') && line.endsWith('%')) {
            QString c = line.mid(1, line.length() - 2);

            if (c.startsWith("FS")) return processFormatSpec(c);
            if (c.startsWith("MO")) {
                m_state.units = c.contains("IN") ? GerberUnit::Inches : GerberUnit::Millimeters;
                return true;
            }
            if (c.startsWith("AD")) return processAperture(c);
            if (c.startsWith("AM")) return true;
            if (c.startsWith("LP")) {
                m_state.polarity = c.contains("D") ? Polarity::Dark : Polarity::Clear;
                return true;
            }
            if (c.startsWith("SR") || c.startsWith("AS") ||
                c.startsWith("IN") || c.startsWith("TF") ||
                c.startsWith("TA") || c.startsWith("TO") || c.startsWith("TD"))
                return true;

            return true;
        }

    if (line.startsWith("G04")) return true;

    QString remaining = line;

    // G-коды
    QRegularExpression gRx("G(\\d{2})");
    auto gm = gRx.match(remaining);
    while (gm.hasMatch()) {
        processGCode(gm.captured(1).toInt());
        remaining.remove(gm.capturedStart(), gm.capturedLength());
        gm = gRx.match(remaining);
    }

    // D-коды
// D-коды — поддерживаем D3, D03, D10 и т.д.
    QRegularExpression dRx("D(\\d{1,3})");
    auto dm = dRx.match(remaining);
    if (dm.hasMatch()) {
        processDCode(dm.captured(1).toInt());
        remaining.remove(dm.capturedStart(), dm.capturedLength());
    }

    // Регионы G36/G37
    if (line.contains("G36")) {
        m_state.inRegion = true;
        m_state.regionPoints.clear();
  ///      qDebug() << "Region start at line" << lineNum;
        return true;
    }
    if (line.contains("G37")) {
        if (m_state.inRegion && m_state.regionPoints.size() > 2)
            closeRegion();
        m_state.inRegion = false;
  ///      qDebug() << "Region end at line" << lineNum;
        return true;
    }

    // ========== ВСПЫШКИ D3 / D03 ==========
    if ((line.contains("D3") || line.contains("D03")) && (line.contains('X') || line.contains('Y'))) {
        QPointF flashPoint = parseCurrentCoordinate(remaining);
     ///   qDebug() << "=== FLASH ===";
     ///   qDebug() << "  Line:" << line;
   ///     qDebug() << "  Point:" << flashPoint;
    ///    qDebug() << "  CurrentAperture:" << m_state.currentAperture;
    ///    qDebug() << "  Has aperture in map:" << m_state.apertures.contains(m_state.currentAperture);

        double r = 0.1;  // 0.1 мм по умолчанию
        if (m_state.currentAperture > 0 && m_state.apertures.contains(m_state.currentAperture)) {
            const Aperture& apt = m_state.apertures[m_state.currentAperture];
            qDebug() << "  Aperture D" << m_state.currentAperture
                << "type:" << (int)apt.type
                << "sizeX:" << apt.sizeX
                << "sizeY:" << apt.sizeY;

            r = apt.sizeX / 2.0;

            if (apt.type == ApertureType::Circle) {
                QPolygonF circle;
                for (int i = 0; i <= 24; ++i) {
                    double angle = 2.0 * M_PI * i / 24.0;
                    circle << QPointF(flashPoint.x() + r * cos(angle),
                        flashPoint.y() + r * sin(angle));
                }
                m_layer.polygons.append(circle);
  ///              qDebug() << "  -> CIRCLE r=" << r;
            }
            else {
                double w = apt.sizeX;
                double h = apt.sizeY > 0 ? apt.sizeY : apt.sizeX;
                QPolygonF rect;
                rect << QPointF(flashPoint.x() - w / 2, flashPoint.y() - h / 2)
                    << QPointF(flashPoint.x() + w / 2, flashPoint.y() - h / 2)
                    << QPointF(flashPoint.x() + w / 2, flashPoint.y() + h / 2)
                    << QPointF(flashPoint.x() - w / 2, flashPoint.y() + h / 2);
                m_layer.polygons.append(rect);
  ///              qDebug() << "  -> RECT" << w << "x" << h;
            }
        }
        else if (m_state.apertures.contains(10)) {
            // Апертура D10 по умолчанию
            const Aperture& apt = m_state.apertures[10];
            r = apt.sizeX / 2.0;
            QPolygonF circle;
            for (int i = 0; i <= 24; ++i) {
                double angle = 2.0 * M_PI * i / 24.0;
                circle << QPointF(flashPoint.x() + r * cos(angle),
                    flashPoint.y() + r * sin(angle));
            }
            m_layer.polygons.append(circle);
   ///         qDebug() << "  -> CIRCLE D10 r=" << r;
        }
        else {
            // Микроточка
            QPolygonF dot;
            dot << QPointF(flashPoint.x() - 0.02, flashPoint.y() - 0.02)
                << QPointF(flashPoint.x() + 0.02, flashPoint.y() - 0.02)
                << QPointF(flashPoint.x() + 0.02, flashPoint.y() + 0.02)
                << QPointF(flashPoint.x() - 0.02, flashPoint.y() + 0.02);
            m_layer.polygons.append(dot);
  ///          qDebug() << "  -> DOT";
        }

        m_state.currentPoint = flashPoint;
        return true;
    }
    return true;

}

bool Gerber_Parser::processFormatSpec(const QString& spec) {
    QRegularExpression rx("FS[LTIAN]?X(\\d)(\\d)Y(\\d)(\\d)");
    auto m = rx.match(spec);
    if (m.hasMatch()) {
        m_state.intDigits = m.captured(1).toInt();
        m_state.fracDigits = m.captured(2).toInt();
        return true;
    }
    return false;
}

bool Gerber_Parser::processAperture(const QString& def) {
    qDebug() << "=== processAperture ===";
    qDebug() << "  def:" << def;

    // Убираем % и *
    QString s = def;
    s.remove('%');
    s.remove('*');

    // Находим первую цифру — начало ID
    int start = 0;
    while (start < s.length() && !s[start].isDigit()) start++;

    // Извлекаем ID
    int id = 10;
    int end = start;
    while (end < s.length() && s[end].isDigit()) end++;
    if (end > start) id = s.mid(start, end - start).toInt();

    // После ID идёт буква формы (C, R, O, P)
    QString shape;
    int pos = end;
    if (pos < s.length() && s[pos].isLetter()) {
        shape = s[pos];
        pos++;
    }

    // Всё остальное — параметры
    QString paramsStr = s.mid(pos);
    paramsStr = paramsStr.trimmed();
    if (paramsStr.startsWith(',')) paramsStr = paramsStr.mid(1);
    paramsStr = paramsStr.trimmed();

    // Разделяем по X
    QStringList params = paramsStr.split('X', Qt::SkipEmptyParts);
    for (auto& p : params) p = p.trimmed();

    qDebug() << "  ID:" << id << "Shape:" << shape;
    qDebug() << "  Params:" << params;

    Aperture apt;
    apt.id = id;
    apt.sizeX = 0.1;
    apt.sizeY = 0.1;

    if (shape == "R") {
        apt.type = ApertureType::Rectangle;
        if (!params.isEmpty()) apt.sizeX = params[0].toDouble();
        if (params.size() > 1) apt.sizeY = params[1].toDouble();
        qDebug() << "  -> RECT:" << apt.sizeX << "x" << apt.sizeY;
    }
    else if (shape == "O") {
        apt.type = ApertureType::Obround;
        if (!params.isEmpty()) apt.sizeX = params[0].toDouble();
        if (params.size() > 1) apt.sizeY = params[1].toDouble();
        qDebug() << "  -> OBROUND:" << apt.sizeX << "x" << apt.sizeY;
    }
    else {
        apt.type = ApertureType::Circle;
        if (!params.isEmpty()) {
            apt.sizeX = params[0].toDouble();
            apt.sizeY = apt.sizeX;
        }
        qDebug() << "  -> CIRCLE:" << apt.sizeX;
    }

    if (apt.sizeY <= 0.0) apt.sizeY = apt.sizeX;
    if (apt.sizeX <= 0.0) apt.sizeX = 0.1;
    if (apt.sizeY <= 0.0) apt.sizeY = 0.1;

    m_state.apertures[id] = apt;
    qDebug() << "  STORED: D" << id << "sizeX:" << apt.sizeX << "sizeY:" << apt.sizeY;
    return true;
}

bool Gerber_Parser::processGCode(int code) {
    switch (code) {
    case 1:  m_state.interpolation = InterpolationMode::Linear; break;
    case 2:  m_state.interpolation = InterpolationMode::Clockwise; break;
    case 3:  m_state.interpolation = InterpolationMode::CounterClockwise; break;
    case 36: m_state.inRegion = true; m_state.regionPoints.clear(); break;
    case 37:
        if (m_state.inRegion && m_state.regionPoints.size() > 2) closeRegion();
        m_state.inRegion = false; break;
    case 70: m_state.units = GerberUnit::Inches; break;
    case 71: m_state.units = GerberUnit::Millimeters; break;
    case 74: m_state.quadrantMode = 0; break;
    case 75: m_state.quadrantMode = 1; break;
    case 90: m_state.absoluteMode = true; break;
    case 91: m_state.absoluteMode = false; break;
    }
    return true;
}

bool Gerber_Parser::processDCode(int code) {
    if (code >= 10) {
        if (!m_state.apertures.contains(code)) {
            Aperture a;
            a.id = code;
            a.sizeX = 0.1;
            a.sizeY = 0.1;
            m_state.apertures[code] = a;
        }
        m_state.currentAperture = code;
        return true;
    }
    return true;
}

void Gerber_Parser::addLineSegment(const QPointF& from, const QPointF& to) {
    if (from == to) return;
    GerberSegment seg;
    seg.startPoint = from; seg.endPoint = to;
    seg.mode = InterpolationMode::Linear;
    seg.apertureId = m_state.currentAperture;
    if (m_state.currentAperture > 0 && m_state.apertures.contains(m_state.currentAperture))
        seg.width = m_state.apertures[m_state.currentAperture].sizeX;
    m_layer.segments.append(seg);
}

void Gerber_Parser::addArcSegment(const QPointF& from, const QPointF& to,
    const QPointF& center, bool cw) {
    GerberSegment seg;
    seg.startPoint = from; seg.endPoint = to; seg.centerPoint = center;
    seg.mode = cw ? InterpolationMode::Clockwise : InterpolationMode::CounterClockwise;
    seg.apertureId = m_state.currentAperture;
    m_layer.segments.append(seg);
}

void Gerber_Parser::closeRegion() {
    if (m_state.regionPoints.size() < 3) {
        m_state.regionPoints.clear();
        return;
    }

    QPolygonF poly;
    for (auto& p : m_state.regionPoints) poly << p;
    if (poly.first() != poly.last()) poly << poly.first();

    // Убираем коллинеарные точки
    QPolygonF simp;
    for (int i = 0; i < poly.size(); ++i) {
        int pv = (i - 1 + poly.size()) % poly.size();
        int nx = (i + 1) % poly.size();
        double cross = (poly[i].x() - poly[pv].x()) * (poly[nx].y() - poly[i].y()) -
            (poly[i].y() - poly[pv].y()) * (poly[nx].x() - poly[i].x());
        if (fabs(cross) > 0.001) simp << poly[i];
    }

    if (simp.size() >= 3) {
        m_layer.polygons.append(simp);
///        qDebug() << "Region closed:" << simp.size() << "points";
    }

    m_state.regionPoints.clear();
}

double Gerber_Parser::gerberNumberToDouble(const QString& numStr, bool) {
    QString s = numStr;
    s.remove('*');
    bool neg = s.startsWith('-');
    if (neg) s = s.mid(1);

    // Если есть точка — парсим как обычное число
    if (s.contains('.')) {
        return s.toDouble() * (neg ? -1.0 : 1.0);
    }

    // Gerber-формат без точки
    int total = m_state.intDigits + m_state.fracDigits;
    while (s.length() < total) s.prepend('0');
    double v = s.left(m_state.intDigits).toDouble() +
        s.mid(m_state.intDigits, m_state.fracDigits).toDouble() / pow(10.0, m_state.fracDigits);
    return neg ? -v : v;
}

QPointF Gerber_Parser::parseCurrentCoordinate(const QString& coordStr) {
    QPointF pt = m_state.currentPoint;

    // Ищем X и Y
    int xi = coordStr.indexOf('X');
    int yi = coordStr.indexOf('Y');

    double x = pt.x();
    double y = pt.y();

    if (xi >= 0) {
        QString xStr;
        int end = coordStr.indexOf('Y', xi + 1);
        if (end < 0) end = coordStr.indexOf('D', xi + 1);
        if (end < 0) end = coordStr.indexOf('*', xi + 1);
        if (end < 0) end = coordStr.length();
        xStr = coordStr.mid(xi + 1, end - xi - 1);
        xStr.remove('*');

        // ПРОСТО: число в миллиметрах, делим на 100
        x = xStr.toDouble() / 1000.0;
    }

    if (yi >= 0) {
        QString yStr;
        int end = coordStr.indexOf('D', yi + 1);
        if (end < 0) end = coordStr.indexOf('*', yi + 1);
        if (end < 0) end = coordStr.length();
        yStr = coordStr.mid(yi + 1, end - yi - 1);
        yStr.remove('*');

        y = yStr.toDouble() / 1000.0;
    }

    if (!m_state.absoluteMode) {
        x += pt.x();
        y += pt.y();
    }

    qDebug() << "RAW coord:" << coordStr.left(30) << "-> X:" << x << "Y:" << y;
    return QPointF(x, y);
}
