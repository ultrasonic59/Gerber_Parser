#ifndef GERBERPARSER_H
#define GERBERPARSER_H

#include <QObject>
#include "gerbertypes.h"

class Gerber_Parser : public QObject {
    Q_OBJECT
public:
    explicit Gerber_Parser(QObject* parent = nullptr);
    GerberLayer parseFile(const QString& filePath, const GerberParseOptions& opts = GerberParseOptions());
    GerberLayer parseText(const QString& text, const QString& displayName = QString(),
        const GerberParseOptions& opts = GerberParseOptions());
    QString lastError() const { return m_lastError; }

signals:
    void parseProgress(int percent);

private:
    struct ParserState {
        QPointF currentPoint;
        GerberUnit units = GerberUnit::Millimeters;
        QMap<int, Aperture> apertures;
        int currentAperture = -1;
        Polarity polarity = Polarity::Dark;
        InterpolationMode interpolation = InterpolationMode::Linear;
        bool inRegion = false;
        QVector<QPointF> regionPoints;
        bool absoluteMode = true;
        int intDigits = 2, fracDigits = 4, quadrantMode = 0;
        void reset();
    };

    QString m_lastError;
    ParserState m_state;
    GerberLayer m_layer;

    bool processLine(const QString& line, int lineNum);
    bool processDCode(int code);
    bool processGCode(int code);
    bool processCoordinate(const QString& coordStr);
    bool processAperture(const QString& defStr);
    bool processFormatSpec(const QString& spec);
    void addLineSegment(const QPointF& from, const QPointF& to);
    void addArcSegment(const QPointF& from, const QPointF& to, const QPointF& center, bool clockwise);
    void closeRegion();
    double gerberNumberToDouble(const QString& numStr, bool isInteger = false);
    QPointF parseCurrentCoordinate(const QString& coordStr);
    void clear();
    GerberLayer parseContent(const QString& content, const QString& displayName,
        const GerberParseOptions& opts);
    void finalizeLayer();
};

#endif
