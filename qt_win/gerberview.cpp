#include "gerberview.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtMath>

GerberView::GerberView(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void GerberView::setEditMode(bool enabled) {
    m_editMode = enabled;
    update();
}

void GerberView::applyViewTransformFrom(const GerberLayer& previous) {
    if (m_layer.boundingRect.isEmpty())
        return;
    if (previous.boundingRect.isEmpty()) {
        fitInView();
        return;
    }
    const double oldW = previous.boundingRect.width();
    const double newW = m_layer.boundingRect.width();
    if (oldW > 1e-6 && newW > 1e-6) {
        const double ratio = newW / oldW;
        m_scale *= ratio;
    }
}

void GerberView::setLayer(const GerberLayer& layer, bool keepViewTransform) {
    const GerberLayer previous = m_layer;
    m_layer = layer;
    m_selected = VertexHit();
    m_dragVertex = VertexHit();

    if (!keepViewTransform && !m_layer.boundingRect.isEmpty())
        fitInView();
    else if (keepViewTransform)
        applyViewTransformFrom(previous);

    update();
}

void GerberView::clear() {
    m_layer = GerberLayer();
    m_scale = 1.0;
    m_offset = QPointF();
    m_selected = VertexHit();
    update();
}

void GerberView::fitInView() {
    if (m_layer.boundingRect.isEmpty()) return;

    const double margin = 10;
    const double scaleX = (width() - 2 * margin) / m_layer.boundingRect.width();
    const double scaleY = (height() - 2 * margin) / m_layer.boundingRect.height();
    m_scale = qMin(scaleX, scaleY);
    if (m_scale < 0.1) m_scale = 0.1;
    if (m_scale > 100) m_scale = 100;

    const QPointF center = m_layer.boundingRect.center();
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

GerberView::VertexHit GerberView::hitTestVertex(const QPointF& screenPos, double* distOut) const {
    VertexHit best;
    double bestDist = kVertexPickPx;

    for (int pi = 0; pi < m_layer.polygons.size(); ++pi) {
        const QPolygonF& poly = m_layer.polygons[pi];
        const int count = poly.size();
        if (count < 1)
            continue;

        const bool closed = (count > 2 && poly.first() == poly.last());
        const int lastIdx = closed ? count - 2 : count - 1;

        for (int vi = 0; vi <= lastIdx; ++vi) {
            const double d = QLineF(screenPos, worldToScreen(poly[vi])).length();
            if (d < bestDist) {
                bestDist = d;
                best.polyIndex = pi;
                best.pointIndex = vi;
            }
        }
    }

    if (distOut)
        *distOut = best.valid() ? bestDist : -1.0;
    return best;
}

bool GerberView::hitTestEdge(const QPointF& screenPos, VertexHit* hitOut, QPointF* insertWorld) const {
    double bestDist = kEdgePickPx;
    bool found = false;
    VertexHit edgeHit;
    QPointF bestPoint;

    for (int pi = 0; pi < m_layer.polygons.size(); ++pi) {
        const QPolygonF& poly = m_layer.polygons[pi];
        const int count = poly.size();
        if (count < 2)
            continue;

        const bool closed = (count > 2 && poly.first() == poly.last());
        const int segCount = closed ? count - 1 : count - 1;

        for (int si = 0; si < segCount; ++si) {
            const QPointF a = worldToScreen(poly[si]);
            const QPointF b = worldToScreen(poly[(si + 1) % count]);
            const QLineF seg(a, b);
            if (seg.length() < 1e-3)
                continue;

            const double t = qBound(0.0, QPointF::dotProduct(screenPos - a, seg.p2() - seg.p1()) / seg.length() / seg.length(), 1.0);
            const QPointF proj = a + t * (b - a);
            const double d = QLineF(screenPos, proj).length();
            if (d < bestDist) {
                bestDist = d;
                edgeHit.polyIndex = pi;
                edgeHit.pointIndex = si + 1;
                bestPoint = screenToWorld(proj);
                found = true;
            }
        }
    }

    if (found) {
        if (hitOut)
            *hitOut = edgeHit;
        if (insertWorld)
            *insertWorld = bestPoint;
    }
    return found;
}

void GerberView::emitLayerEdited() {
    m_layer.recomputeBounds();
    if (!m_layer.boundingRect.isEmpty())
        m_layer.boundingRect.adjust(-2, -2, 2, 2);
    emit layerEdited(m_layer);
    emitSelectionChanged();
    update();
}

void GerberView::emitSelectionChanged() {
    if (m_selected.valid())
        emit selectionChanged(m_selected.polyIndex,
            m_selected.hasVertex() ? m_selected.pointIndex : -1);
    else
        emit selectionChanged(-1, -1);
}

void GerberView::setSelectedVertex(int polyIndex, int pointIndex) {
    if (polyIndex < 0 || polyIndex >= m_layer.polygons.size()) {
        m_selected = VertexHit();
        update();
        return;
    }
    const QPolygonF& poly = m_layer.polygons[polyIndex];
    if (pointIndex < 0 || pointIndex >= poly.size())
        pointIndex = 0;
    m_selected = { polyIndex, pointIndex };
    update();
}

int GerberView::hitTestPolygon(const QPointF& screenPos) const {
    const QPointF world = screenToWorld(screenPos);
    int found = -1;

    for (int pi = 0; pi < m_layer.polygons.size(); ++pi) {
        const QPolygonF& poly = m_layer.polygons[pi];
        if (poly.size() < 3)
            continue;
        if (poly.containsPoint(world, Qt::OddEvenFill))
            found = pi;
    }
    return found;
}

void GerberView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(20, 25, 30));

    if (m_layer.valid && !m_layer.boundingRect.isEmpty())
        drawGrid(painter);

    drawPolygons(painter);
    drawSegments(painter);

    if (m_editMode)
        drawVertices(painter);

    if (m_layer.valid) {
        painter.setPen(QColor(200, 200, 200));
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);

        const QPointF mouseWorld = screenToWorld(mapFromGlobal(QCursor::pos()));
        QString hint = m_editMode
            ? "Edit: click polygon | drag vertex | dbl-click edge add | Del remove | Shift+click new"
            : "View only";
        painter.drawText(10, height() - 36, hint);
        painter.drawText(10, height() - 18,
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

    const QPointF topLeft = screenToWorld(QPointF(0, 0));
    const QPointF bottomRight = screenToWorld(QPointF(width(), height()));

    double gridStep = 5.0;
    while (gridStep * m_scale < 20.0) gridStep *= 2.0;
    while (gridStep * m_scale > 100.0) gridStep /= 2.0;

    painter.setPen(QPen(QColor(60, 70, 80), 0.5));

    for (double x = floor(topLeft.x() / gridStep) * gridStep;
        x < bottomRight.x(); x += gridStep) {
        const QPointF p1 = worldToScreen(QPointF(x, topLeft.y()));
        const QPointF p2 = worldToScreen(QPointF(x, bottomRight.y()));
        painter.drawLine(p1, p2);
    }

    for (double y = floor(topLeft.y() / gridStep) * gridStep;
        y < bottomRight.y(); y += gridStep) {
        const QPointF p1 = worldToScreen(QPointF(topLeft.x(), y));
        const QPointF p2 = worldToScreen(QPointF(bottomRight.x(), y));
        painter.drawLine(p1, p2);
    }
}

void GerberView::drawPolygons(QPainter& painter) {
    painter.save();
    painter.setBrush(QBrush(QColor(0, 150, 255, 120)));
    painter.setPen(QPen(QColor(0, 100, 200), 2.0));

    for (int pi = 0; pi < m_layer.polygons.size(); ++pi) {
        const QPolygonF& poly = m_layer.polygons[pi];
        if (poly.size() < 2)
            continue;

        const bool polySelected = (m_selected.polyIndex == pi);
        if (polySelected) {
            painter.setBrush(QBrush(QColor(255, 180, 60, 150)));
            painter.setPen(QPen(QColor(255, 140, 40), 3.0));
        } else {
            painter.setBrush(QBrush(QColor(0, 150, 255, 120)));
            painter.setPen(QPen(QColor(0, 100, 200), 2.0));
        }

        QPolygonF screenPoly;
        for (const QPointF& pt : poly)
            screenPoly << worldToScreen(pt);

        if (poly.size() >= 3)
            painter.drawPolygon(screenPoly);
        else
            painter.drawPolyline(screenPoly);
    }
    painter.restore();
}

void GerberView::drawSegments(QPainter& painter) {
    for (const GerberSegment& seg : m_layer.segments) {
        const QPointF p1 = worldToScreen(seg.startPoint);
        const QPointF p2 = worldToScreen(seg.endPoint);

        double w = seg.width > 0 ? seg.width * m_scale : 2.0;
        if (w < 1.0) w = 1.0;
        if (w > 8.0) w = 8.0;

        painter.setPen(QPen(QColor(255, 220, 100), w));
        painter.drawLine(p1, p2);
    }
}

void GerberView::drawVertices(QPainter& painter) {
    for (int pi = 0; pi < m_layer.polygons.size(); ++pi) {
        const QPolygonF& poly = m_layer.polygons[pi];
        const bool closed = (poly.size() > 2 && poly.first() == poly.last());
        const int lastIdx = closed ? poly.size() - 2 : poly.size() - 1;

        for (int vi = 0; vi <= lastIdx; ++vi) {
            const QPointF sp = worldToScreen(poly[vi]);
            const bool selected = (m_selected.polyIndex == pi && m_selected.hasVertex()
                && m_selected.pointIndex == vi);
            painter.setPen(QPen(selected ? QColor(255, 80, 80) : QColor(255, 255, 255), 2));
            painter.setBrush(selected ? QColor(255, 120, 120) : QColor(255, 200, 80));
            painter.drawEllipse(sp, 5, 5);
        }
    }
}

void GerberView::wheelEvent(QWheelEvent *e) {
    double f = 1.15;
    if (e->angleDelta().y() < 0) f = 1.0 / f;
    const QPointF mw = screenToWorld(e->position());
    m_scale *= f;
    if (m_scale < 0.01) m_scale = 0.01;
    if (m_scale > 1000.0) m_scale = 1000.0;
    m_offset += e->position() - worldToScreen(mw);
    update();
}

void GerberView::mousePressEvent(QMouseEvent *e) {
    if (m_editMode && e->button() == Qt::LeftButton) {
        const VertexHit hit = hitTestVertex(e->pos());
        if (hit.valid()) {
            m_selected = hit;
            m_dragVertex = hit;
            m_draggingVertex = true;
            m_lastPos = e->pos();
            setCursor(Qt::SizeAllCursor);
            emitSelectionChanged();
            update();
            return;
        }

        const int polyIdx = hitTestPolygon(e->pos());
        if (polyIdx >= 0 && !(e->modifiers() & Qt::ShiftModifier)) {
            m_selected.polyIndex = polyIdx;
            m_selected.pointIndex = -1;
            emitSelectionChanged();
            update();
            return;
        }

        if (e->modifiers() & Qt::ShiftModifier) {
            QPolygonF tri;
            const QPointF w = screenToWorld(e->pos());
            tri << w << w + QPointF(1, 0) << w + QPointF(0, 1);
            m_layer.polygons.append(tri);
            m_selected = { m_layer.polygons.size() - 1, 0 };
            emitLayerEdited();
            return;
        }
    }

    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_lastPos = e->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void GerberView::mouseMoveEvent(QMouseEvent *e) {
    if (m_draggingVertex && m_dragVertex.hasVertex()) {
        QPolygonF& poly = m_layer.polygons[m_dragVertex.polyIndex];
        poly[m_dragVertex.pointIndex] = screenToWorld(e->pos());

        if (poly.size() > 2 && poly.first() == poly.last()) {
            if (m_dragVertex.pointIndex == 0)
                poly[poly.size() - 1] = poly[0];
            else if (m_dragVertex.pointIndex == poly.size() - 1)
                poly[0] = poly[poly.size() - 1];
        }
        update();
        return;
    }

    if (m_dragging) {
        m_offset += e->pos() - m_lastPos;
        m_lastPos = e->pos();
    }
    update();
}

void GerberView::mouseReleaseEvent(QMouseEvent *e) {
    if (m_draggingVertex) {
        m_draggingVertex = false;
        setCursor(Qt::ArrowCursor);
        emitLayerEdited();
        emitSelectionChanged();
        return;
    }

    if (e->button() == Qt::LeftButton || e->button() == Qt::MiddleButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void GerberView::mouseDoubleClickEvent(QMouseEvent *e) {
    if (!m_editMode || e->button() != Qt::LeftButton)
        return;

    VertexHit edgeHit;
    QPointF worldPt;
    if (!hitTestEdge(e->pos(), &edgeHit, &worldPt))
        return;

    QPolygonF& poly = m_layer.polygons[edgeHit.polyIndex];
    poly.insert(edgeHit.pointIndex, worldPt);
    m_selected = { edgeHit.polyIndex, edgeHit.pointIndex };
    emitLayerEdited();
}

void GerberView::keyPressEvent(QKeyEvent *e) {
    if (!m_editMode || !m_selected.hasVertex())
        return QWidget::keyPressEvent(e);

    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        QPolygonF& poly = m_layer.polygons[m_selected.polyIndex];
        const bool closed = (poly.size() > 2 && poly.first() == poly.last());
        const int minPts = closed ? 4 : 3;

        if (poly.size() < minPts)
            return;

        poly.remove(m_selected.pointIndex);
        if (closed) {
            poly[0] = poly.last();
            if (m_selected.pointIndex >= poly.size() - 1)
                m_selected.pointIndex = 0;
        } else if (m_selected.pointIndex >= poly.size()) {
            m_selected.pointIndex = poly.size() - 1;
        }

        emitLayerEdited();
        return;
    }

    QWidget::keyPressEvent(e);
}

void GerberView::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
}
