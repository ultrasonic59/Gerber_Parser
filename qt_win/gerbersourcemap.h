#ifndef GERBERSOURCEMAP_H
#define GERBERSOURCEMAP_H

#include "gerbertypes.h"
#include <QVector>
#include <QPointF>

struct GerberTextSpan {
    int start = 0;
    int end = 0;
    int lineNumber = 0;
    QPointF fileMm;
    int polyIndex = -1;
    int pointIndex = -1;
    int segmentIndex = -1;
    int regionPolyIndex = -1;
    bool hasCoords = false;
    bool isRegionStart = false;
    bool isRegionEnd = false;
};

struct GerberHighlightRange {
    int start = 0;
    int end = 0;
    bool primary = false;
};

class GerberSourceMap {
public:
    void rebuild(const QString& text, const GerberLayer& layer);

    QVector<GerberHighlightRange> rangesForVertex(int polyIndex, int pointIndex) const;
    QVector<GerberHighlightRange> rangesForPolygon(int polyIndex) const;

    bool vertexAtCursor(int charPos, const GerberLayer& layer, int* polyIndex, int* pointIndex) const;
    const QVector<GerberTextSpan>& spans() const { return m_spans; }

private:
    static bool parseCoordLine(const QString& line, QPointF* fileMm);
    void linkSpansToLayer(const GerberLayer& layer);

    QVector<GerberTextSpan> m_spans;
    static constexpr double kMatchToleranceMm = 0.15;
};

#endif
