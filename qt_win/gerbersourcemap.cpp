#include "gerbersourcemap.h"
#include "gerberwriter.h"
#include <QLineF>
#include <QRegularExpression>
#include <QtMath>

namespace {

QPolygonF polygonInFileSpace(const GerberLayer& layer, int polyIndex) {
    QPolygonF filePoly;
    if (polyIndex < 0 || polyIndex >= layer.polygons.size())
        return filePoly;
    for (const QPointF& local : layer.polygons[polyIndex])
        filePoly << GerberWriter::toFileCoords(local, layer);
    return filePoly;
}

int findPolygonContainingFilePoint(const QPointF& filePt, const GerberLayer& layer) {
    int found = -1;
    for (int pi = 0; pi < layer.polygons.size(); ++pi) {
        const QPolygonF filePoly = polygonInFileSpace(layer, pi);
        if (filePoly.size() < 3)
            continue;
        if (filePoly.containsPoint(filePt, Qt::OddEvenFill))
            found = pi;
    }
    return found;
}

int findPolygonNearestFilePoint(const QPointF& filePt, const GerberLayer& layer, double maxDistMm) {
    int bestPi = -1;
    double bestDist = maxDistMm;

    for (int pi = 0; pi < layer.polygons.size(); ++pi) {
        const QPolygonF& localPoly = layer.polygons[pi];
        if (localPoly.isEmpty())
            continue;

        QPointF centroid(0, 0);
        for (const QPointF& p : localPoly)
            centroid += p;
        centroid /= localPoly.size();
        centroid = GerberWriter::toFileCoords(centroid, layer);

        double d = QLineF(filePt, centroid).length();
        if (d < bestDist) {
            bestDist = d;
            bestPi = pi;
        }

        const bool closed = (localPoly.size() > 2 && localPoly.first() == localPoly.last());
        const int lastIdx = closed ? localPoly.size() - 2 : localPoly.size() - 1;
        for (int vi = 0; vi <= lastIdx; ++vi) {
            const QPointF vtx = GerberWriter::toFileCoords(localPoly[vi], layer);
            d = QLineF(filePt, vtx).length();
            if (d < bestDist) {
                bestDist = d;
                bestPi = pi;
            }
        }
    }
    return bestPi;
}

int spanPolygonIndex(const GerberTextSpan& span) {
    if (span.polyIndex >= 0)
        return span.polyIndex;
    return span.regionPolyIndex;
}

} // namespace

bool GerberSourceMap::parseCoordLine(const QString& line, QPointF* fileMm) {
    if (!fileMm)
        return false;

    const int xi = line.indexOf('X', 0, Qt::CaseInsensitive);
    const int yi = line.indexOf('Y', 0, Qt::CaseInsensitive);
    if (xi < 0 || yi < 0)
        return false;

    auto extractNumber = [&](int axisPos) -> double {
        QString num;
        for (int i = axisPos + 1; i < line.size(); ++i) {
            const QChar c = line[i];
            if (c.isDigit() || c == '.' || c == '-' || c == '+')
                num.append(c);
            else if (!num.isEmpty())
                break;
        }
        if (num.isEmpty())
            return 0.0;
        return num.toDouble() / 1000.0;
    };

    fileMm->setX(extractNumber(xi));
    fileMm->setY(extractNumber(yi));
    return true;
}

void GerberSourceMap::rebuild(const QString& text, const GerberLayer& layer) {
    m_spans.clear();
    if (text.isEmpty())
        return;

    int charPos = 0;
    int lineNumber = 0;
    int activeRegionPoly = -1;
    int nextRegionPoly = 0;

    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (const QString& rawLine : lines) {
        lineNumber++;
        QString lineContent = rawLine;
        if (lineContent.endsWith(QLatin1Char('\r')))
            lineContent.chop(1);

        const int lineStart = charPos;
        const int lineEnd = charPos + lineContent.length();

        GerberTextSpan span;
        span.start = lineStart;
        span.end = lineEnd;
        span.lineNumber = lineNumber;

        const QString trimmed = lineContent.trimmed();
        if (trimmed.contains(QLatin1String("G36"), Qt::CaseInsensitive)) {
            span.isRegionStart = true;
            activeRegionPoly = nextRegionPoly++;
            span.regionPolyIndex = activeRegionPoly;
            span.polyIndex = activeRegionPoly;
        } else if (trimmed.contains(QLatin1String("G37"), Qt::CaseInsensitive)) {
            span.isRegionEnd = true;
            span.regionPolyIndex = activeRegionPoly;
            span.polyIndex = activeRegionPoly;
            activeRegionPoly = -1;
        } else if (activeRegionPoly >= 0) {
            span.regionPolyIndex = activeRegionPoly;
            span.polyIndex = activeRegionPoly;
        }

        QPointF filePt;
        if (parseCoordLine(trimmed, &filePt)) {
            span.hasCoords = true;
            span.fileMm = filePt;
        }

        if (!trimmed.isEmpty() || span.isRegionStart || span.isRegionEnd)
            m_spans.append(span);

        charPos = lineEnd + 1;
    }

    linkSpansToLayer(layer);
}

void GerberSourceMap::linkSpansToLayer(const GerberLayer& layer) {
    const double vertexTol = 0.15;

    for (GerberTextSpan& span : m_spans) {
        if (!span.hasCoords)
            continue;

        int byContain = findPolygonContainingFilePoint(span.fileMm, layer);
        if (byContain >= 0) {
            span.polyIndex = byContain;
            span.regionPolyIndex = byContain;
            continue;
        }

        for (int pi = 0; pi < layer.polygons.size(); ++pi) {
            const QPolygonF& localPoly = layer.polygons[pi];
            const bool closed = (localPoly.size() > 2 && localPoly.first() == localPoly.last());
            const int lastIdx = closed ? localPoly.size() - 2 : localPoly.size() - 1;

            for (int vi = 0; vi <= lastIdx; ++vi) {
                const QPointF expected = GerberWriter::toFileCoords(localPoly[vi], layer);
                if (QLineF(span.fileMm, expected).length() <= vertexTol) {
                    span.polyIndex = pi;
                    span.pointIndex = vi;
                    span.regionPolyIndex = pi;
                    break;
                }
            }
            if (span.polyIndex >= 0)
                break;
        }

        if (span.polyIndex < 0) {
            const int nearPi = findPolygonNearestFilePoint(span.fileMm, layer, 2.0);
            if (nearPi >= 0) {
                span.polyIndex = nearPi;
                span.regionPolyIndex = nearPi;
            }
        }
    }

    for (GerberTextSpan& span : m_spans) {
        if (span.polyIndex < 0 && span.regionPolyIndex >= 0
            && span.regionPolyIndex < layer.polygons.size()) {
            span.polyIndex = span.regionPolyIndex;
        }
    }
}

QVector<GerberHighlightRange> GerberSourceMap::rangesForVertex(int polyIndex, int pointIndex) const {
    QVector<GerberHighlightRange> ranges;
    if (polyIndex < 0)
        return ranges;

    for (const GerberTextSpan& span : m_spans) {
        if (spanPolygonIndex(span) != polyIndex)
            continue;

        GerberHighlightRange r;
        r.start = span.start;
        r.end = qMax(span.start + 1, span.end);
        r.primary = (pointIndex >= 0 && span.hasCoords && span.pointIndex == pointIndex);

        if (pointIndex < 0)
            ranges.append(r);
        else if (r.primary || span.isRegionStart || span.isRegionEnd
            || (span.hasCoords && span.pointIndex < 0))
            ranges.append(r);
        else if (!span.hasCoords)
            ranges.append(r);
    }

    return ranges;
}

QVector<GerberHighlightRange> GerberSourceMap::rangesForPolygon(int polyIndex) const {
    return rangesForVertex(polyIndex, -1);
}

bool GerberSourceMap::vertexAtCursor(int charPos, const GerberLayer& layer, int* polyIndex, int* pointIndex) const {
    if (!polyIndex || !pointIndex)
        return false;

    for (const GerberTextSpan& span : m_spans) {
        if (charPos < span.start || charPos > span.end)
            continue;

        const int pi = spanPolygonIndex(span);
        if (pi < 0)
            continue;

        *polyIndex = pi;
        *pointIndex = span.pointIndex;

        if (span.hasCoords && span.pointIndex < 0) {
            const QPolygonF filePoly = polygonInFileSpace(layer, pi);
            if (filePoly.size() >= 3) {
                for (int vi = 0; vi < filePoly.size(); ++vi) {
                    if (QLineF(span.fileMm, filePoly[vi]).length() <= kMatchToleranceMm) {
                        *pointIndex = vi;
                        break;
                    }
                }
            }
        }
        return true;
    }
    return false;
}
