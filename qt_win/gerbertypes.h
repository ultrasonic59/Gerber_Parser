#ifndef GEBERTYPES_H
#define GEBERTYPES_H

#include <QString>
#include <QVector>
#include <QPointF>
#include <QPolygonF>
#include <QMap>

enum class ApertureType { Circle, Rectangle, Obround, Polygon, Macro };
enum class InterpolationMode { Linear, Clockwise, CounterClockwise };
enum class GerberUnit { Inches, Millimeters };
enum class Polarity { Dark, Clear };

struct Aperture {
    int id = 10;
    ApertureType type = ApertureType::Circle;
    double sizeX = 0.0, sizeY = 0.0, holeSize = 0.0, rotation = 0.0;
    int sides = 4;
    QString macroName;
};

struct GerberSegment {
    QPointF startPoint, endPoint, centerPoint;
    InterpolationMode mode = InterpolationMode::Linear;
    int apertureId = -1;
    double width = 0.0;
};

struct GerberCommand {
    QString rawText;
    int lineNumber = 0;
};

struct GerberLayer {
    QString fileName;
    QVector<QPolygonF> polygons;
    QVector<GerberSegment> segments;
    QVector<GerberCommand> commands;
    QRectF boundingRect;
    GerberUnit units = GerberUnit::Millimeters;
    QString errorMessage;
    bool valid = false;
};

struct GerberParseOptions {
    bool debugOutput = false;
    bool mergePolygons = true;
};

#endif
