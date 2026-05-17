#include "gerbertypes.h"

void GerberLayer::recomputeBounds() {
    boundingRect = QRectF();
    for (const QPolygonF& poly : polygons)
        boundingRect |= poly.boundingRect();
    for (const GerberSegment& seg : segments) {
        boundingRect |= QRectF(seg.startPoint, seg.endPoint).normalized();
        if (seg.mode != InterpolationMode::Linear)
            boundingRect |= QRectF(seg.centerPoint, QSizeF(0, 0));
    }
}
