#ifndef GERBERVIEW_H
#define GERBERVIEW_H

#include <QWidget>
#include "gerbertypes.h"

class GerberView : public QWidget
{
    Q_OBJECT
public:
    explicit GerberView(QWidget *parent = nullptr);
    void setLayer(const GerberLayer &layer);
    void clear();
    void fitInView();

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void drawSegments(QPainter &);
    void drawPolygons(QPainter &);
    void drawGrid(QPainter &);
    QPointF screenToWorld(const QPointF &s) const;
    QPointF worldToScreen(const QPointF &w) const;

    GerberLayer m_layer;
    double m_scale = 1.0;
    QPointF m_offset;
    bool m_dragging = false;
    QPoint m_lastPos;
    double m_gridSize = 10.0;
};

#endif
