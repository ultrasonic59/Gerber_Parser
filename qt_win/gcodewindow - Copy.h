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
#include "gerbertypes.h"

class GCodeView : public QWidget {
    Q_OBJECT
public:
    explicit GCodeView(QWidget* parent = nullptr);
    void setPoints(const QVector<QPointF>& points, const QRectF& bounds);
    void resetView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QVector<QPointF> m_points;
    QRectF m_bounds;
    double m_scale = 1.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
};

class GCodeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit GCodeWindow(QWidget* parent = nullptr);
    void setGCode(const QString& gcode, const QVector<QPointF>& points, const QRectF& bounds);

private:
    QTextEdit* m_textEdit;
    GCodeView* m_gcodeView;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_resetViewBtn;
    QLabel* m_infoLabel;
    QSplitter* m_splitter;
};

#endif // GCODEWINDOW_H
