#ifndef GCODEWINDOW_H
#define GCODEWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QSplitter>
#include <QVector>
#include <QPointF>
#include <QWidget>
#include <QAction>
#include <QMenu>
#include "gerbertypes.h"

class GCodeView : public QWidget {
    Q_OBJECT
public:
    explicit GCodeView(QWidget* parent = nullptr);
    void setPoints(const QVector<QPointF>& points, const QRectF& bounds);
    const QVector<QPointF>& points() const { return m_points; }
    void resetView();
    void deleteSelectedPoint();
    void moveSelectedPoint(const QPointF& newPos);

signals:
    void pointsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QVector<QPointF> m_points;
    QRectF m_bounds;
    double m_scale = 1.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_dragging = false;
    bool m_draggingPoint = false;
    int m_draggedPointIndex = -1;
    int m_selectedPointIndex = -1;
    QPoint m_lastMousePos;

    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
    int findNearestPoint(const QPointF& screenPos, double maxDist = 10.0) const;
};

class GCodeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit GCodeWindow(QWidget* parent = nullptr);
    void setGCode(const QString& gcode, const QVector<QPointF>& points, const QRectF& bounds);
    QString regenerateGCode(const QVector<QPointF>& points);

private:
    QTextEdit* m_textEdit;
    GCodeView* m_gcodeView;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_resetViewBtn;
    QPushButton* m_deletePointBtn;
    QLabel* m_infoLabel;
    QSplitter* m_splitter;
    QString m_originalComment;
};

#endif // GCODEWINDOW_H
