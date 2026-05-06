#ifndef GCODEGENERATOR_H
#define GCODEGENERATOR_H

#include <QString>
#include <QVector>
#include <QPointF>
#include "gerbertypes.h"

struct DispensePoint {
    QPointF position;
    double diameter;      // мм — диаметр дозы
    double dispenseTime;  // мс — время дозирования
    double height;        // мм — высота подъёма
};

struct GCodePointMap {
    // Все номера строк 0-based (как в QPlainTextEdit::textCursor().blockNumber()).
    int startLine = -1; // строка комментария "; Point N ..."
    int xyLine = -1;    // строка "G00 X.. Y.."
    int endLine = -1;   // последняя строка, относящаяся к точке
};

class GCodeGenerator {
public:
    GCodeGenerator();

    // Настройки
    void setFeedRate(double mmPerMin) { m_feedRate = mmPerMin; }
    void setZUp(double mm) { m_zUp = mm; }
    void setZDown(double mm) { m_zDown = mm; }
    void setDwellTime(int ms) { m_dwellTime = ms; }
    void setXYOffset(double x, double y) { m_offsetX = x; m_offsetY = y; }

    // Генерация из слоя Gerber
    QVector<DispensePoint> generateDispensePoints(const GerberLayer &layer);

    // Экспорт G-кода
    QString generateGCode(const QVector<DispensePoint> &points, const QString &comment = "");
    QString generateGCode(const QVector<DispensePoint> &points, QVector<GCodePointMap>* pointMap, const QString &comment = "");

    // Получить boundingRect для графического вывода
    QRectF getBoundingRect() const { return m_boundingRect; }

    double computeDispenseTime(double diameter) const;

private:
    double m_feedRate = 3000.0;  // мм/мин
    double m_zUp = 5.0;          // мм
    double m_zDown = 0.2;        // мм
    int m_dwellTime = 150;       // мс
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    QRectF m_boundingRect;

    double calculateDispenseTime(double diameter) const;
    QString formatCoord(double value) const;
};

#endif // GCODEGENERATOR_H
