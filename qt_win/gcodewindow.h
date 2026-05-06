#ifndef GCODEWINDOW_H
#define GCODEWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QDockWidget>
#include <QAction>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QSplitter>
#include <QVector>
#include <QPointF>
#include <QPoint>
#include <QWidget>
#include <QColor>
#include <QRect>
#include <QRectF>
#include <QEvent>
#include "gerbertypes.h"
#include "gcodegenerator.h"

class QPainter;

class GCodeView : public QWidget {
    Q_OBJECT
public:
    explicit GCodeView(QWidget* parent = nullptr);
    void setPoints(const QVector<QPointF>& points, const QRectF& bounds);
    void setSelectedIndices(const QVector<int>& indices);
    void resetView();

    void setBackgroundColor(const QColor& c);
    void setGridColor(const QColor& c);
    void setPointColor(const QColor& c);
    void setSelectedPointColor(const QColor& c);
    void setGridAuto(bool on);
    void setFixedGridStep(double mm);
    void setMeasureMode(bool on);
    bool measureMode() const { return m_measureMode; }

    QColor backgroundColor() const { return m_bgColor; }
    QColor gridColor() const { return m_gridColor; }
    QColor pointColor() const { return m_pointColor; }
    QColor selectedPointColor() const { return m_selectedPointColor; }
    bool gridAuto() const { return m_gridAuto; }
    double fixedGridStep() const { return m_fixedGridStep; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QVector<QPointF> m_points;
    QVector<int> m_selected;
    QRectF m_bounds;
    double m_scale = 1.0;
    double m_offsetX = 0.0;
    double m_offsetY = 0.0;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    bool m_movingSelection = false;
    QPointF m_moveAnchorWorld;
    QPointF m_lastMoveWorld;
    bool m_rubberBandActive = false;
    QPoint m_rubberOrigin;
    QPoint m_rubberCurrent;
    QPoint m_leftDragStartPos;
    bool m_panStartedOnEmpty = false;
    bool m_measureMode = false;
    int m_measureState = 0;
    QPointF m_measureP0;
    QPointF m_measureP1;
    bool m_measurePreviewValid = false;
    QPointF m_measurePreviewWorld;
    QColor m_bgColor{ 18, 22, 28 };
    QColor m_gridColor{ 35, 42, 50 };
    QColor m_pointColor{ 255, 200, 50 };
    QColor m_selectedPointColor{ 255, 90, 90 };
    bool m_gridAuto = true;
    double m_fixedGridStep = 5.0;
    static constexpr int kRulerTopPx = 22;
    static constexpr int kRulerLeftPx = 48;
    static constexpr int kPlotPadPx = 6;
    void mapParams(double* outScale, double* outOx, double* outOy) const;
    void drawRulers(QPainter& painter, double gridStep) const;
    QRect plotRect() const;
    QRectF visibleWorldRect() const;
    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
    int pickPointIndex(const QPoint& screenPos, double* outDistPx = nullptr) const;
    static QColor darkerOutline(const QColor& fill, int factor = 140);

signals:
    void selectionChanged(const QVector<int>& indices);
    void requestMoveSelectedBy(const QPointF& deltaWorld);
    void requestCommitMove();
    void requestAddPointAt(const QPointF& worldPos, int insertAfterIndex);
    void requestDeleteSelected();
    void cursorWorldPosChanged(double x, double y, bool valid);
    void measureTapeChanged(const QString& statusText);
};

class GCodeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit GCodeWindow(QWidget* parent = nullptr);
    void setGCode(const QVector<DispensePoint>& points, const QRectF& bounds, const QString& comment = "");

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QTextEdit* m_textEdit;
    GCodeView* m_gcodeView;
    QPushButton* m_copyBtn;
    QPushButton* m_saveBtn;
    QPushButton* m_resetViewBtn;
    QLabel* m_infoLabel;
    QLabel* m_xyLabel;
    QLabel* m_measureTapeLabel;
    QSplitter* m_splitter;
    QDockWidget* m_selectionDock;
    QTableWidget* m_selectionTable;
    QAction* m_actShowSelectionDock;
    QAction* m_actMeasureTape;

    GCodeGenerator m_gen;
    QVector<DispensePoint> m_points;
    QRectF m_bounds;
    QString m_comment;
    QVector<GCodePointMap> m_pointMap;
    QVector<int> m_selected;

    void rebuildTextAndView();
    void setSelection(const QVector<int>& indices, bool ensureVisibleInText);
    void highlightSelectionInText();
    void scrollToPointInText(int pointIndex);
    QRectF computeBoundsFromPoints() const;
    void setupViewMenu();
    void updateSelectionPanel();
};

#endif // GCODEWINDOW_H
