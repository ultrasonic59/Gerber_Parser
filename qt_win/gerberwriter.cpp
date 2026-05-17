#include "gerberwriter.h"
#include <cmath>

QString GerberWriter::formatCoord(double mmValue) {
    const qint64 microns = qRound64(mmValue * 1000.0);
    return QString::number(microns);
}

QPointF GerberWriter::toFileCoords(const QPointF& local, const GerberLayer& layer) {
    return local + layer.coordOffset;
}

QString GerberWriter::layerToText(const GerberLayer& layer) {
    QString out;
    out += "G04 Edited by Gerber Parser*\n";
    out += "%FSLAX24Y24*%\n";
    out += layer.units == GerberUnit::Inches ? "%MOIN*%\n" : "%MOMM*%\n";
    out += "%ADD10C,0.100*%\n";
    out += "D10*\n";
    out += "G01*\n";

    auto writePoint = [&](const QPointF& local, bool draw) {
        const QPointF filePt = toFileCoords(local, layer);
        out += QString("X%1Y%2%3*\n")
            .arg(formatCoord(filePt.x()))
            .arg(formatCoord(filePt.y()))
            .arg(draw ? "D01" : "D02");
    };

    for (const QPolygonF& poly : layer.polygons) {
        if (poly.size() < 2)
            continue;

        out += "G36*\n";
        writePoint(poly.first(), false);

        const int n = poly.size();
        const bool closed = (n > 2 && poly.first() == poly.last());
        const int last = closed ? n - 2 : n - 1;

        for (int i = 1; i <= last; ++i)
            writePoint(poly[i], true);

        if (!closed && poly.size() >= 3)
            writePoint(poly.first(), true);

        out += "G37*\n";
    }

    for (const GerberSegment& seg : layer.segments) {
        writePoint(seg.startPoint, false);
        writePoint(seg.endPoint, true);
    }

    out += "M02*\n";
    return out;
}
