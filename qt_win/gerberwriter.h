#ifndef GERBERWRITER_H
#define GERBERWRITER_H

#include "gerbertypes.h"

class GerberWriter {
public:
    static QString layerToText(const GerberLayer& layer);
    static QString formatCoord(double mmValue);
    static QPointF toFileCoords(const QPointF& local, const GerberLayer& layer);
};

#endif
