#include "gerberview.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <cmath>
#include <QDebug>

GerberView::GerberView(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void GerberView::setLayer(const GerberLayer& layer) {
    m_layer = layer;

    if (!m_layer.boundingRect.isEmpty()) {
        // Авто-масштаб: подгоняем под размер окна
        double scaleX = width() * 0.8 / m_layer.boundingRect.width();
        double scaleY = height() * 0.8 / m_layer.boundingRect.height();
        m_scale = qMin(scaleX, scaleY);
        if (m_scale < 0.1) m_scale = 0.1;
        if (m_scale > 100) m_scale = 100;

        // Центрируем
        QPointF center = m_layer.boundingRect.center();
        m_offset = QPointF(width() / 2.0 - center.x() * m_scale,
            height() / 2.0 - center.y() * m_scale);
    }

    update();
}

void GerberView::clear() {
    m_layer = GerberLayer();
    m_scale = 1.0;
    m_offset = QPointF();
    update();
}

void GerberView::fitInView() {
    if (m_layer.boundingRect.isEmpty()) return;

    double margin = 10;
    double scaleX = (width() - 2 * margin) / m_layer.boundingRect.width();
    double scaleY = (height() - 2 * margin) / m_layer.boundingRect.height();
    m_scale = qMin(scaleX, scaleY);
    if (m_scale < 0.1) m_scale = 0.1;
    if (m_scale > 100) m_scale = 100;

    QPointF center = m_layer.boundingRect.center();
    m_offset = QPointF(width() / 2.0 - center.x() * m_scale,
        height() / 2.0 - center.y() * m_scale);
    update();
}

QPointF GerberView::screenToWorld(const QPointF& screen) const {
    return QPointF((screen.x() - m_offset.x()) / m_scale,
        (screen.y() - m_offset.y()) / m_scale);
}

QPointF GerberView::worldToScreen(const QPointF& world) const {
    return QPointF(world.x() * m_scale + m_offset.x(),
        world.y() * m_scale + m_offset.y());
}

void GerberView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Фон
    painter.fillRect(rect(), QColor(20, 25, 30));

    // Сетка (только если слой загружен)
    if (m_layer.valid && !m_layer.boundingRect.isEmpty())
        drawGrid(painter);

    // Полигоны (сначала)
    drawPolygons(painter);

    // Сегменты (поверх полигонов)
    drawSegments(painter);

    // Информация
    if (m_layer.valid) {
        painter.setPen(QColor(200, 200, 200));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);

        QPointF mouseWorld = screenToWorld(mapFromGlobal(QCursor::pos()));
        painter.drawText(10, height() - 20,
            QString("Scale: %1x | X:%2 Y:%3 | Polys:%4 Segs:%5")
            .arg(m_scale, 0, 'f', 2)
            .arg(mouseWorld.x(), 0, 'f', 2)
            .arg(mouseWorld.y(), 0, 'f', 2)
            .arg(m_layer.polygons.size())
            .arg(m_layer.segments.size()));
    }
}

void GerberView::drawGrid(QPainter& painter) {
    if (m_layer.boundingRect.isEmpty()) return;

    // Видимая область в мировых координатах
    QPointF topLeft = screenToWorld(QPointF(0, 0));
    QPointF bottomRight = screenToWorld(QPointF(width(), height()));

    // Адаптивный шаг сетки
    double gridStep = 5.0;
    while (gridStep * m_scale < 20.0) gridStep *= 2.0;
    while (gridStep * m_scale > 100.0) gridStep /= 2.0;

    painter.setPen(QPen(QColor(60, 70, 80), 0.5));

    // Вертикальные линии
    for (double x = floor(topLeft.x() / gridStep) * gridStep;
        x < bottomRight.x(); x += gridStep) {
        QPointF p1 = worldToScreen(QPointF(x, topLeft.y()));
        QPointF p2 = worldToScreen(QPointF(x, bottomRight.y()));
        if (p1.x() >= 0 && p1.x() <= width() && p2.x() >= 0 && p2.x() <= width())
            painter.drawLine(p1, p2);
    }

    // Горизонтальные линии
    for (double y = floor(topLeft.y() / gridStep) * gridStep;
        y < bottomRight.y(); y += gridStep) {
        QPointF p1 = worldToScreen(QPointF(topLeft.x(), y));
        QPointF p2 = worldToScreen(QPointF(bottomRight.x(), y));
        if (p1.y() >= 0 && p1.y() <= height() && p2.y() >= 0 && p2.y() <= height())
            painter.drawLine(p1, p2);
    }
}

void GerberView::drawPolygons(QPainter& painter) {
    painter.save();

    // Яркие цвета для отладки
    painter.setBrush(QBrush(QColor(0, 150, 255, 120)));  // Голубая заливка
    painter.setPen(QPen(QColor(0, 100, 200), 2.0));

    for (int i = 0; i < m_layer.polygons.size(); ++i) {
        const QPolygonF& poly = m_layer.polygons[i];

        if (poly.size() < 3) {
            qDebug() << "Skipping polygon" << i << "size:" << poly.size();
            continue;
        }

        QPolygonF screenPoly;
        for (const QPointF& pt : poly) {
            screenPoly << worldToScreen(pt);
        }

        painter.drawPolygon(screenPoly);

        // Отладка: рамка вокруг каждого полигона
        painter.setPen(QPen(Qt::red, 1.0));
        painter.drawRect(screenPoly.boundingRect());
        painter.setPen(QPen(QColor(0, 100, 200), 2.0));
    }

    painter.restore();
///    qDebug() << "Drew" << m_layer.polygons.size() << "polygons";
}

void GerberView::drawSegments(QPainter& painter) {
    for (const GerberSegment& seg : m_layer.segments) {
        QPointF p1 = worldToScreen(seg.startPoint);
        QPointF p2 = worldToScreen(seg.endPoint);

        double width = seg.width > 0 ? seg.width * m_scale : 2.0;
        if (width < 1.0) width = 1.0;
        if (width > 8.0) width = 8.0;

        painter.setPen(QPen(QColor(255, 220, 100), width));
        painter.drawLine(p1, p2);
    }
}

void GerberView::wheelEvent(QWheelEvent *e) {
    double f = 1.15;
    if (e->angleDelta().y() < 0) f = 1.0 / f;
    QPointF mw = screenToWorld(e->position());
    m_scale *= f;
    if (m_scale < 0.01) m_scale = 0.01;
    if (m_scale > 1000.0) m_scale = 1000.0;
    m_offset += e->position() - worldToScreen(mw);
    update();
}

void GerberView::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastPos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void GerberView::mouseMoveEvent(QMouseEvent *e) {
    if (m_dragging) {
        m_offset += e->pos() - m_lastPos;
        m_lastPos = e->pos();
    }
    update();
}

void GerberView::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void GerberView::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    if (m_layer.valid && !m_layer.boundingRect.isEmpty())
        fitInView();
}
