#ifndef GERBERVIEW_H
#define GERBERVIEW_H

#include <QWidget>
#include "gerbertypes.h"

class GerberView : public QWidget
{
    Q_OBJECT
public:
    struct VertexHit {
        int polyIndex = -1;
        int pointIndex = -1;
        bool valid() const { return polyIndex >= 0; }
        bool hasVertex() const { return polyIndex >= 0 && pointIndex >= 0; }
    };

    explicit GerberView(QWidget *parent = nullptr);
    void setLayer(const GerberLayer &layer, bool keepViewTransform = false);
    GerberLayer layer() const { return m_layer; }
    void clear();
    void fitInView();
    void setEditMode(bool enabled);
    bool editMode() const { return m_editMode; }
    VertexHit selectedVertex() const { return m_selected; }
    void setSelectedVertex(int polyIndex, int pointIndex);

signals:
    void layerEdited(const GerberLayer& layer);
    void selectionChanged(int polyIndex, int pointIndex);

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void drawSegments(QPainter &);
    void drawPolygons(QPainter &);
    void drawVertices(QPainter &);
    void drawGrid(QPainter &);
    QPointF screenToWorld(const QPointF &s) const;
    QPointF worldToScreen(const QPointF &w) const;
    VertexHit hitTestVertex(const QPointF& screenPos, double* distOut = nullptr) const;
    bool hitTestEdge(const QPointF& screenPos, VertexHit* hitOut, QPointF* insertWorld = nullptr) const;
    int hitTestPolygon(const QPointF& screenPos) const;
    void emitLayerEdited();
    void emitSelectionChanged();
    void applyViewTransformFrom(const GerberLayer& previous);

    GerberLayer m_layer;
    double m_scale = 1.0;
    QPointF m_offset;
    bool m_dragging = false;
    bool m_draggingVertex = false;
    QPoint m_lastPos;
    VertexHit m_selected;
    VertexHit m_dragVertex;
    bool m_editMode = true;
    static constexpr double kVertexPickPx = 10.0;
    static constexpr double kEdgePickPx = 8.0;
};

#endif
