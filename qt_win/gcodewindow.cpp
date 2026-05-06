#include "gcodewindow.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTextBlock>
#include <QKeyEvent>
#include <QScrollBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QEvent>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QSizePolicy>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>

// ==================== GCodeView ====================

GCodeView::GCodeView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(340, 280);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void GCodeView::mapParams(double* outScale, double* outOx, double* outOy) const {
    if (!outScale || !outOx || !outOy) return;
    if (m_bounds.isEmpty()) {
        *outScale = 0;
        *outOx = 0;
        *outOy = 0;
        return;
    }
    const double plotW = qMax(10.0, double(width() - kRulerLeftPx - kPlotPadPx));
    const double plotH = qMax(10.0, double(height() - kRulerTopPx - kPlotPadPx));
    const double sc = qMin(plotW / m_bounds.width(), plotH / m_bounds.height()) * m_scale;
    *outScale = sc;
    *outOx = kRulerLeftPx + kPlotPadPx - m_bounds.left() * sc + m_offsetX;
    *outOy = kRulerTopPx + kPlotPadPx - m_bounds.top() * sc + m_offsetY;
}

QRect GCodeView::plotRect() const {
    return QRect(kRulerLeftPx, kRulerTopPx,
        qMax(1, width() - kRulerLeftPx - kPlotPadPx),
        qMax(1, height() - kRulerTopPx - kPlotPadPx));
}

QRectF GCodeView::visibleWorldRect() const {
    if (m_bounds.isEmpty()) return QRectF();
    const QRect pr = plotRect();
    const QPointF c1 = screenToWorld(QPointF(pr.left(), pr.top()));
    const QPointF c2 = screenToWorld(QPointF(pr.right(), pr.top()));
    const QPointF c3 = screenToWorld(QPointF(pr.left(), pr.bottom()));
    const QPointF c4 = screenToWorld(QPointF(pr.right(), pr.bottom()));
    const double xmin = qMin(qMin(c1.x(), c2.x()), qMin(c3.x(), c4.x()));
    const double xmax = qMax(qMax(c1.x(), c2.x()), qMax(c3.x(), c4.x()));
    const double ymin = qMin(qMin(c1.y(), c2.y()), qMin(c3.y(), c4.y()));
    const double ymax = qMax(qMax(c1.y(), c2.y()), qMax(c3.y(), c4.y()));
    return QRectF(QPointF(xmin, ymin), QPointF(xmax, ymax));
}

void GCodeView::drawRulers(QPainter& painter, double gridStep) const {
    double sc = 0, ox = 0, oy = 0;
    mapParams(&sc, &ox, &oy);
    if (sc <= 0 || gridStep <= 0) return;

    const QColor rulerBg = m_bgColor.darker(108);
    const QColor cornerBg = m_bgColor.darker(118);
    const QColor tickColor = m_gridColor.lighter(125);
    const QColor textColor(160, 170, 180);

    painter.fillRect(0, 0, width(), kRulerTopPx, rulerBg);
    painter.fillRect(0, kRulerTopPx, kRulerLeftPx, height() - kRulerTopPx, rulerBg);
    painter.fillRect(0, 0, kRulerLeftPx, kRulerTopPx, cornerBg);

    const QPointF wTL = screenToWorld(QPointF(kRulerLeftPx + kPlotPadPx, kRulerTopPx + kPlotPadPx));
    const QPointF wBR = screenToWorld(QPointF(width() - kPlotPadPx, height() - kPlotPadPx));
    const double xMin = qMin(wTL.x(), wBR.x());
    const double xMax = qMax(wTL.x(), wBR.x());
    const double yMin = qMin(wTL.y(), wBR.y());
    const double yMax = qMax(wTL.y(), wBR.y());

    auto decimalsForStep = [](double s) -> int {
        if (s < 0.09) return 3;
        if (s < 0.9) return 2;
        if (s < 9) return 1;
        return 0;
    };
    const int dec = decimalsForStep(gridStep);

    painter.setFont(QFont(painter.font().family(), 7));

    painter.setPen(QPen(tickColor, 1));
    double lastLabelSx = -1e9;
    for (double x = std::floor(xMin / gridStep) * gridStep; x <= xMax + gridStep * 1e-6; x += gridStep) {
        const double sx = x * sc + ox;
        if (sx < -5 || sx > width() + 5) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(sx, 0), QPointF(sx, kRulerTopPx - 1));
        if (sx - lastLabelSx > 34 && sx >= kRulerLeftPx - 2) {
            painter.setPen(textColor);
            const QString txt = QString::number(x, 'f', dec);
            painter.drawText(QRectF(sx - 24, 1, 48, kRulerTopPx - 3), Qt::AlignHCenter | Qt::AlignVCenter, txt);
            lastLabelSx = sx;
        }
    }

    double lastLabelSy = -1e9;
    for (double y = std::floor(yMin / gridStep) * gridStep; y <= yMax + gridStep * 1e-6; y += gridStep) {
        const double sy = y * sc + oy;
        if (sy < -5 || sy > height() + 5) continue;
        painter.setPen(QPen(tickColor, 1));
        painter.drawLine(QPointF(0, sy), QPointF(kRulerLeftPx - 1, sy));
        // Не рисуем подпись Y в зоне угла с «mm», иначе накладывается на единицу измерения
        if (sy > kRulerTopPx + 16 && std::abs(sy - lastLabelSy) > 22) {
            painter.save();
            painter.translate(11, sy);
            painter.rotate(-90);
            painter.setPen(textColor);
            const QString txt = QString::number(y, 'f', dec);
            painter.drawText(QRectF(-36, -8, 72, 16), Qt::AlignCenter, txt);
            painter.restore();
            lastLabelSy = sy;
        }
    }

    painter.setPen(QPen(m_gridColor, 1));
    painter.drawLine(kRulerLeftPx, 0, kRulerLeftPx, height());
    painter.drawLine(0, kRulerTopPx, width(), kRulerTopPx);

    painter.setPen(textColor);
    painter.setFont(QFont(painter.font().family(), 7, QFont::Bold));
    painter.drawText(QRect(1, 1, kRulerLeftPx - 2, kRulerTopPx - 1),
        Qt::AlignRight | Qt::AlignBottom, QStringLiteral("mm"));
}

QColor GCodeView::darkerOutline(const QColor& fill, int factor) {
    return fill.darker(factor);
}

void GCodeView::setBackgroundColor(const QColor& c) {
    m_bgColor = c;
    update();
}

void GCodeView::setGridColor(const QColor& c) {
    m_gridColor = c;
    update();
}

void GCodeView::setPointColor(const QColor& c) {
    m_pointColor = c;
    update();
}

void GCodeView::setSelectedPointColor(const QColor& c) {
    m_selectedPointColor = c;
    update();
}

void GCodeView::setGridAuto(bool on) {
    m_gridAuto = on;
    update();
}

void GCodeView::setFixedGridStep(double mm) {
    m_fixedGridStep = qMax(0.01, mm);
    m_gridAuto = false;
    update();
}

void GCodeView::setMeasureMode(bool on) {
    m_measureMode = on;
    if (!on) {
        m_measureState = 0;
        m_measurePreviewValid = false;
        emit measureTapeChanged(QString());
    }
    if (!m_dragging && !m_movingSelection && !m_rubberBandActive)
        setCursor(m_measureMode ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void GCodeView::setPoints(const QVector<QPointF>& points, const QRectF& bounds) {
    m_points = points;
    m_bounds = bounds;
    update();
}

void GCodeView::setSelectedIndices(const QVector<int>& indices) {
    m_selected = indices;
    update();
}

void GCodeView::resetView() {
    m_scale = 1.0;
    m_offsetX = 0.0;
    m_offsetY = 0.0;
    update();
}

QPointF GCodeView::worldToScreen(const QPointF& world) const {
    double sc = 0, ox = 0, oy = 0;
    mapParams(&sc, &ox, &oy);
    if (sc <= 0) return QPointF();
    return QPointF(world.x() * sc + ox, world.y() * sc + oy);
}

QPointF GCodeView::screenToWorld(const QPointF& screen) const {
    double sc = 0, ox = 0, oy = 0;
    mapParams(&sc, &ox, &oy);
    if (sc <= 0) return QPointF();
    return QPointF((screen.x() - ox) / sc, (screen.y() - oy) / sc);
}

int GCodeView::pickPointIndex(const QPoint& screenPos, double* outDistPx) const {
    if (m_points.isEmpty() || m_bounds.isEmpty()) return -1;

    double bestD2 = 1e100;
    int bestIdx = -1;
    const QPointF s(screenPos.x(), screenPos.y());
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF sp = worldToScreen(m_points[i]);
        const double dx = sp.x() - s.x();
        const double dy = sp.y() - s.y();
        const double d2 = dx * dx + dy * dy;
        if (d2 < bestD2) {
            bestD2 = d2;
            bestIdx = i;
        }
    }

    const double d = std::sqrt(bestD2);
    if (outDistPx) *outDistPx = d;

    const double pickRadiusPx = 10.0;
    return (d <= pickRadiusPx) ? bestIdx : -1;
}

void GCodeView::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.15 : 0.87;
    QPointF mouseWorld = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
    m_scale *= factor;
    if (m_scale < 0.05) m_scale = 0.05;
    if (m_scale > 50.0) m_scale = 50.0;
    QPointF newScreen = worldToScreen(mouseWorld);
    m_offsetX += event->pos().x() - newScreen.x();
    m_offsetY += event->pos().y() - newScreen.y();
    update();
}

void GCodeView::mousePressEvent(QMouseEvent* event) {
    setFocus();

    if (event->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_panStartedOnEmpty = false;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (m_measureMode && m_measureState > 0) {
            m_measureState = 0;
            m_measurePreviewValid = false;
            emit measureTapeChanged(QString());
            setCursor(Qt::CrossCursor);
            update();
            return;
        }
        m_rubberBandActive = true;
        m_rubberOrigin = event->pos();
        m_rubberCurrent = event->pos();
        m_panStartedOnEmpty = false;
        setCursor(Qt::CrossCursor);
        update();
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    double distPx = 0.0;
    const int hit = pickPointIndex(event->pos(), &distPx);
    const bool ctrl = (event->modifiers() & Qt::ControlModifier);
    const bool shift = (event->modifiers() & Qt::ShiftModifier);

    if (m_measureMode && !shift) {
        m_panStartedOnEmpty = false;
        QPointF w;
        if (hit >= 0) w = m_points[hit];
        else w = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        if (m_measureState != 1) {
            m_measureP0 = w;
            m_measureState = 1;
            m_measurePreviewValid = false;
            emit measureTapeChanged(tr("Рулетка: выберите вторую точку (ЛКМ) или сброс ПКМ / Esc"));
        } else {
            m_measureP1 = w;
            m_measureState = 2;
            m_measurePreviewValid = false;
            const double dx = m_measureP1.x() - m_measureP0.x();
            const double dy = m_measureP1.y() - m_measureP0.y();
            const double dist = std::hypot(dx, dy);
            emit measureTapeChanged(tr("Рулетка: %1 мм  |  ΔX %2  ΔY %3")
                .arg(dist, 0, 'f', 3).arg(dx, 0, 'f', 3).arg(dy, 0, 'f', 3));
        }
        update();
        return;
    }

    if (shift) {
        m_panStartedOnEmpty = false;
        const QPointF w = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        int after = -1;
        for (int idx : m_selected) after = qMax(after, idx);
        emit requestAddPointAt(w, after);
        return;
    }

    if (hit >= 0) {
        m_panStartedOnEmpty = false;
        QVector<int> newSel = m_selected;
        if (ctrl) {
            if (newSel.contains(hit)) newSel.removeAll(hit);
            else newSel.append(hit);
        } else {
            // Без Ctrl: клик по уже выбранной точке сохраняет мультивыделение (для совместного переноса).
            if (!m_selected.contains(hit))
                newSel = { hit };
        }
        std::sort(newSel.begin(), newSel.end());
        emit selectionChanged(newSel);

        const bool alt = (event->modifiers() & Qt::AltModifier);
        if (alt && newSel.contains(hit)) {
            m_movingSelection = true;
            m_moveAnchorWorld = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
            m_lastMoveWorld = m_moveAnchorWorld;
            setCursor(Qt::SizeAllCursor);
        }
        return;
    }

    // Пустое место: панорамирование ЛКМ (рамка выделения — ПКМ).
    m_panStartedOnEmpty = true;
    m_leftDragStartPos = event->pos();
    m_dragging = true;
    m_lastMousePos = event->pos();
    setCursor(Qt::ClosedHandCursor);
}

void GCodeView::mouseMoveEvent(QMouseEvent* event) {
    if (m_rubberBandActive) {
        m_rubberCurrent = event->pos();
        update();
        if (!m_bounds.isEmpty()) {
            QPointF world = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
            emit cursorWorldPosChanged(world.x(), world.y(), true);
        }
        return;
    }

    if (m_movingSelection) {
        const QPointF w = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        const QPointF delta = w - m_lastMoveWorld;
        m_lastMoveWorld = w;
        emit requestMoveSelectedBy(delta);
        if (!m_bounds.isEmpty())
            emit cursorWorldPosChanged(w.x(), w.y(), true);
        return;
    }

    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        m_lastMousePos = event->pos();
        update();
        if (!m_bounds.isEmpty()) {
            QPointF world = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
            emit cursorWorldPosChanged(world.x(), world.y(), true);
        }
        return;
    }

    if (m_measureMode && m_measureState == 1 && !m_bounds.isEmpty()) {
        m_measurePreviewWorld = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        m_measurePreviewValid = true;
        const double dx = m_measurePreviewWorld.x() - m_measureP0.x();
        const double dy = m_measurePreviewWorld.y() - m_measureP0.y();
        const double dist = std::hypot(dx, dy);
        emit measureTapeChanged(tr("Рулетка: %1 мм (предпр.)  |  ΔX %2  ΔY %3")
            .arg(dist, 0, 'f', 3).arg(dx, 0, 'f', 3).arg(dy, 0, 'f', 3));
        update();
    }

    if (m_bounds.isEmpty())
        emit cursorWorldPosChanged(0, 0, false);
    else {
        QPointF world = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        emit cursorWorldPosChanged(world.x(), world.y(), true);
    }
}

void GCodeView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const bool wasMoving = m_movingSelection;
        if (m_panStartedOnEmpty && !wasMoving
            && (event->pos() - m_leftDragStartPos).manhattanLength() <= 6) {
            emit selectionChanged({});
        }
        m_panStartedOnEmpty = false;
        m_movingSelection = false;
        if (m_dragging) m_dragging = false;
        setCursor(m_measureMode ? Qt::CrossCursor : Qt::ArrowCursor);
        if (wasMoving) emit requestCommitMove();
        return;
    }
    if (event->button() == Qt::RightButton) {
        if (m_rubberBandActive) {
            const QRect norm = QRect(m_rubberOrigin, m_rubberCurrent).normalized();
            m_rubberBandActive = false;
            const Qt::KeyboardModifiers km = QApplication::keyboardModifiers();
            const bool addMode = (km & Qt::ControlModifier);
            if (norm.width() >= 4 && norm.height() >= 4) {
                QVector<int> inside;
                inside.reserve(m_points.size());
                for (int i = 0; i < m_points.size(); ++i) {
                    const QPoint sp = worldToScreen(m_points[i]).toPoint();
                    if (norm.contains(sp)) inside.append(i);
                }
                std::sort(inside.begin(), inside.end());
                if (addMode) {
                    QVector<int> merged = m_selected;
                    for (int id : inside) {
                        if (!merged.contains(id)) merged.append(id);
                    }
                    std::sort(merged.begin(), merged.end());
                    emit selectionChanged(merged);
                } else {
                    emit selectionChanged(inside);
                }
            } else {
                if (!addMode)
                    emit selectionChanged({});
            }
            update();
        }
        setCursor(m_measureMode ? Qt::CrossCursor : Qt::ArrowCursor);
        return;
    }
    if (event->button() == Qt::MiddleButton) {
        m_dragging = false;
        m_panStartedOnEmpty = false;
        setCursor(m_measureMode ? Qt::CrossCursor : Qt::ArrowCursor);
        return;
    }
}

void GCodeView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && m_measureMode) {
        m_measureState = 0;
        m_measurePreviewValid = false;
        emit measureTapeChanged(QString());
        update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit requestDeleteSelected();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void GCodeView::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (m_rubberBandActive) {
        m_rubberBandActive = false;
        update();
    }
    emit cursorWorldPosChanged(0, 0, false);
}

void GCodeView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), m_bgColor);

    if (m_points.isEmpty() || m_bounds.isEmpty()) {
        painter.setPen(QColor(100, 100, 100));
        QFont f = painter.font(); f.setPointSize(14); painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, "No dispense points");
        return;
    }

    double sc = 0, ox = 0, oy = 0;
    mapParams(&sc, &ox, &oy);
    double gridStep = 5.0;
    if (m_gridAuto) {
        while (gridStep * sc < 20) gridStep *= 2;
        while (gridStep * sc > 80) gridStep /= 2;
    } else {
        gridStep = m_fixedGridStep;
    }

    painter.save();
    painter.setClipRect(plotRect());

    const QRectF vwr = visibleWorldRect();
    const double wx0 = vwr.left();
    const double wx1 = vwr.right();
    const double wy0 = vwr.top();
    const double wy1 = vwr.bottom();
    const double eps = gridStep * 1e-9;

    // Сетка по видимой области (при зуме заполняет весь plot)
    painter.setPen(QPen(m_gridColor, 0.5));
    for (double x = std::floor(wx0 / gridStep) * gridStep; x <= wx1 + eps; x += gridStep) {
        painter.drawLine(worldToScreen(QPointF(x, wy0)), worldToScreen(QPointF(x, wy1)));
    }
    for (double y = std::floor(wy0 / gridStep) * gridStep; y <= wy1 + eps; y += gridStep) {
        painter.drawLine(worldToScreen(QPointF(wx0, y)), worldToScreen(QPointF(wx1, y)));
    }

    // Маршрут
    painter.setPen(QPen(QColor(0, 180, 100, 80), 1.0, Qt::DashLine));
    for (int i = 0; i < m_points.size() - 1; ++i) {
        painter.drawLine(worldToScreen(m_points[i]), worldToScreen(m_points[i + 1]));
    }

    // Точки
    double pointScale = qMin(sc * 0.3, 8.0);
    if (pointScale < 1.5) pointScale = 1.5;

    painter.setBrush(m_pointColor);
    painter.setPen(QPen(darkerOutline(m_pointColor), 1.0));

    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF& pt = m_points[i];
        QPointF sp = worldToScreen(pt);
        if (sp.x() >= -10 && sp.x() <= width() + 10 &&
            sp.y() >= -10 && sp.y() <= height() + 10) {
            const bool selected = m_selected.contains(i);
            if (selected) {
                painter.setBrush(m_selectedPointColor);
                painter.setPen(QPen(darkerOutline(m_selectedPointColor, 120), 2.0));
                painter.drawEllipse(sp, pointScale + 2.0, pointScale + 2.0);
                painter.setBrush(m_pointColor);
                painter.setPen(QPen(darkerOutline(m_pointColor), 1.0));
            }
            painter.drawEllipse(sp, pointScale, pointScale);
        }
    }

    if (m_measureMode && m_measureState >= 1) {
        const QColor measColor(0, 210, 255);
        const QPointF s0 = worldToScreen(m_measureP0);
        painter.setPen(QPen(measColor, 2));
        painter.setBrush(QColor(measColor.red(), measColor.green(), measColor.blue(), 60));
        painter.drawEllipse(s0, 5, 5);
        if (m_measureState == 2) {
            const QPointF s1 = worldToScreen(m_measureP1);
            painter.setPen(QPen(measColor, 2, Qt::SolidLine));
            painter.drawLine(s0, s1);
            painter.drawEllipse(s1, 5, 5);
            const QPointF mid((s0.x() + s1.x()) * 0.5, (s0.y() + s1.y()) * 0.5);
            const double dx = m_measureP1.x() - m_measureP0.x();
            const double dy = m_measureP1.y() - m_measureP0.y();
            const double dist = std::hypot(dx, dy);
            const QString cap = QStringLiteral("%1 mm").arg(dist, 0, 'f', 3);
            QFont mf = painter.font();
            mf.setPointSize(8);
            mf.setBold(true);
            painter.setFont(mf);
            const QFontMetrics fm(mf);
            painter.save();
            painter.translate(mid);
            const QRect tbr = fm.boundingRect(cap);
            QRectF bg(-tbr.width() / 2.0 - 4, -tbr.height() / 2.0 - 1, tbr.width() + 8, tbr.height() + 2);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(20, 28, 36, 220));
            painter.drawRoundedRect(bg, 3, 3);
            painter.setPen(measColor.lighter(130));
            painter.drawText(bg, Qt::AlignCenter, cap);
            painter.restore();
        } else if (m_measurePreviewValid) {
            const QPointF sp = worldToScreen(m_measurePreviewWorld);
            painter.setPen(QPen(measColor.lighter(140), 1, Qt::DashLine));
            painter.drawLine(s0, sp);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(sp, 4, 4);
        }
    }

    painter.restore();

    if (m_rubberBandActive) {
        const QRect rband = QRect(m_rubberOrigin, m_rubberCurrent).normalized();
        painter.setPen(QPen(QColor(120, 180, 255), 1, Qt::DashLine));
        painter.setBrush(QColor(60, 120, 220, 45));
        painter.drawRect(rband);
    }

    drawRulers(painter, gridStep);

    // Информация
    painter.setPen(QColor(120, 130, 140));
    QFont f = painter.font(); f.setPointSize(9); painter.setFont(f);
    const QString gridLabel = m_gridAuto
        ? QString("auto → %1 mm").arg(gridStep, 0, 'f', 2)
        : QString("%1 mm").arg(gridStep, 0, 'f', 2);
    painter.drawText(kRulerLeftPx + 5, height() - 18,
        QString("Points: %1 | Scale: %2x | Grid: %3")
        .arg(m_points.size())
        .arg(m_scale, 0, 'f', 2)
        .arg(gridLabel));
    painter.drawText(kRulerLeftPx + 5, height() - 5,
        QString("Bounds: %1 x %2 mm")
        .arg(m_bounds.width(), 0, 'f', 1)
        .arg(m_bounds.height(), 0, 'f', 1));
}

// ==================== GCodeWindow ====================

GCodeWindow::GCodeWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("G-Code Generator - Paste Dispenser");
    setMinimumSize(800, 550);
    resize(1000, 650);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_gcodeView = new GCodeView(this);
    m_splitter->addWidget(m_gcodeView);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Consolas", 9));
    m_textEdit->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: none;");
    m_textEdit->setMinimumWidth(250);
    m_splitter->addWidget(m_textEdit);
    connect(m_textEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        if (m_pointMap.isEmpty()) return;
        const int line = m_textEdit->textCursor().blockNumber();
        int hit = -1;
        for (int i = 0; i < m_pointMap.size(); ++i) {
            const auto& pm = m_pointMap[i];
            if (pm.startLine <= line && line <= pm.endLine) { hit = i; break; }
        }
        if (hit >= 0) setSelection({ hit }, false);
    });

    m_splitter->setSizes({ 500, 350 });
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(m_splitter);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 4, 0, 0);

    m_infoLabel = new QLabel("Ready");
    m_infoLabel->setStyleSheet("color: #999;");
    btnLayout->addWidget(m_infoLabel);

    m_xyLabel = new QLabel(tr("X: —  Y: —"), this);
    m_xyLabel->setMinimumWidth(230);
    QFont xyFont = m_xyLabel->font();
    xyFont.setFamily(QStringLiteral("Consolas"));
    xyFont.setPointSize(9);
    m_xyLabel->setFont(xyFont);
    m_xyLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    btnLayout->addWidget(m_xyLabel);

    m_measureTapeLabel = new QLabel(this);
    m_measureTapeLabel->setMinimumWidth(300);
    m_measureTapeLabel->setStyleSheet(QStringLiteral("color: #6ad4ff;"));
    QFont mtFont = m_measureTapeLabel->font();
    mtFont.setPointSize(9);
    m_measureTapeLabel->setFont(mtFont);
    btnLayout->addWidget(m_measureTapeLabel);

    btnLayout->addStretch();

    m_resetViewBtn = new QPushButton("Reset View", this);
    m_resetViewBtn->setToolTip(tr("Сброс масштаба и смещения. Панорама: ЛКМ или СКМ. Рамка: ПКМ. Перенос точек: Alt+ЛКМ."));
    connect(m_resetViewBtn, &QPushButton::clicked, this, [this]() {
        m_gcodeView->resetView();
        });
    btnLayout->addWidget(m_resetViewBtn);

    m_copyBtn = new QPushButton("Copy G-Code", this);
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_textEdit->toPlainText());
        m_infoLabel->setText("Copied to clipboard!");
        });
    btnLayout->addWidget(m_copyBtn);

    m_saveBtn = new QPushButton("Save .gcode", this);
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Save G-Code",
            "dispense.gcode",
            "G-Code (*.gcode *.nc *.cnc);;All (*.*)");
        if (path.isEmpty()) return;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_textEdit->toPlainText();
            file.close();
            m_infoLabel->setText("Saved: " + QFileInfo(path).fileName());
        }
        });
    btnLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(btnLayout);

    m_selectionDock = new QDockWidget(tr("Выделенные точки"), this);
    m_selectionDock->setObjectName(QStringLiteral("SelectionDock"));
    m_selectionDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable
        | QDockWidget::DockWidgetClosable);
    m_selectionDock->setMinimumWidth(400);
    m_selectionDock->setMinimumHeight(160);

    m_selectionTable = new QTableWidget(0, 6);
    m_selectionTable->setHorizontalHeaderLabels({
        tr("№"),
        tr("X, мм"),
        tr("Y, мм"),
        tr("Диам, мм"),
        tr("Z, мм"),
        tr("Доза, мс")
    });
    m_selectionTable->verticalHeader()->setVisible(false);
    m_selectionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_selectionTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_selectionTable->setAlternatingRowColors(true);
    m_selectionTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_selectionTable->setMinimumHeight(120);
    m_selectionTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_selectionTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_selectionTable->horizontalHeader()->setMinimumSectionSize(48);
    for (int c = 0; c < 5; ++c)
        m_selectionTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_selectionTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    QWidget* dockWrap = new QWidget(m_selectionDock);
    QVBoxLayout* dockLay = new QVBoxLayout(dockWrap);
    dockLay->setContentsMargins(2, 2, 2, 2);
    dockLay->addWidget(m_selectionTable);
    m_selectionDock->setWidget(dockWrap);

    addDockWidget(Qt::RightDockWidgetArea, m_selectionDock);
    m_selectionDock->setVisible(false);

    m_actShowSelectionDock = new QAction(tr("Панель выделенных точек"), this);
    m_actShowSelectionDock->setCheckable(true);
    m_actShowSelectionDock->setChecked(false);
    connect(m_actShowSelectionDock, &QAction::toggled, this, [this](bool on) {
        m_selectionDock->setVisible(on);
    });
    connect(m_selectionDock, &QDockWidget::visibilityChanged, this, [this](bool vis) {
        if (m_actShowSelectionDock->isChecked() != vis)
            m_actShowSelectionDock->setChecked(vis);
        if (vis) {
            updateSelectionPanel();
            const int target = qBound(m_selectionDock->minimumWidth(), width() / 3, 560);
            if (width() >= target + 350)
                resizeDocks({ m_selectionDock }, { target }, Qt::Horizontal);
        }
    });

    connect(m_gcodeView, &GCodeView::cursorWorldPosChanged, this, [this](double x, double y, bool valid) {
        if (!valid)
            m_xyLabel->setText(tr("X: —  Y: —"));
        else
            m_xyLabel->setText(tr("X: %1 mm  Y: %2 mm").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
    });

    connect(m_gcodeView, &GCodeView::measureTapeChanged, m_measureTapeLabel, &QLabel::setText);

    m_actMeasureTape = new QAction(tr("Рулетка измерения"), this);
    m_actMeasureTape->setCheckable(true);
    m_actMeasureTape->setToolTip(tr("Два щелчка ЛКМ: расстояние и ΔX, ΔY (мм). ПКМ или Esc — сброс. Привязка к точке при наведении."));
    connect(m_actMeasureTape, &QAction::toggled, this, [this](bool on) {
        m_gcodeView->setMeasureMode(on);
        if (!on) m_measureTapeLabel->clear();
    });

    connect(m_gcodeView, &GCodeView::selectionChanged, this, [this](const QVector<int>& indices) {
        // Не прокручивать текст при мультивыделении: иначе cursorPositionChanged
        // оставит в выделении только одну точку (строку под курсором).
        setSelection(indices, indices.size() <= 1);
    });
    connect(m_gcodeView, &GCodeView::requestMoveSelectedBy, this, [this](const QPointF& deltaWorld) {
        if (m_selected.isEmpty()) return;
        for (int idx : m_selected) {
            if (idx >= 0 && idx < m_points.size())
                m_points[idx].position += deltaWorld;
        }
        // Важно: во время drag НЕ пересоздаём текст и НЕ меняем масштаб/центр.
        QVector<QPointF> positions;
        positions.reserve(m_points.size());
        for (const auto& p : m_points) positions.append(p.position);
        m_gcodeView->setPoints(positions, m_bounds);
        m_gcodeView->setSelectedIndices(m_selected);
        updateSelectionPanel();
    });
    connect(m_gcodeView, &GCodeView::requestCommitMove, this, [this]() {
        // После отпускания мыши — пересобираем G-code (один раз).
        rebuildTextAndView();
    });
    connect(m_gcodeView, &GCodeView::requestAddPointAt, this, [this](const QPointF& worldPos, int insertAfterIndex) {
        DispensePoint p;
        p.position = worldPos;
        p.diameter = 0.5;
        p.dispenseTime = m_gen.computeDispenseTime(p.diameter);
        p.height = 0.2;

        int insertPos = qBound(-1, insertAfterIndex, m_points.size() - 1) + 1;
        m_points.insert(insertPos, p);
        m_bounds = computeBoundsFromPoints();
        rebuildTextAndView();
        setSelection({ insertPos }, true);
    });
    connect(m_gcodeView, &GCodeView::requestDeleteSelected, this, [this]() {
        if (m_selected.isEmpty()) return;
        QVector<int> toDel = m_selected;
        std::sort(toDel.begin(), toDel.end());
        for (int i = toDel.size() - 1; i >= 0; --i) {
            const int idx = toDel[i];
            if (idx >= 0 && idx < m_points.size()) m_points.removeAt(idx);
        }
        m_selected.clear();
        // Не сужаем m_bounds — иначе меняется «базовый» масштаб отображения.
        if (m_points.isEmpty()) {
            m_bounds = QRectF();
        } else {
            const QRectF pb = computeBoundsFromPoints();
            if (!pb.isEmpty()) m_bounds = m_bounds.united(pb);
        }
        rebuildTextAndView();
        setSelection({}, false);
    });

    setupViewMenu();
}

void GCodeWindow::setGCode(const QVector<DispensePoint>& points, const QRectF& bounds, const QString& comment) {
    m_points = points;
    m_bounds = bounds;
    m_comment = comment;
    m_selected.clear();
    if (m_bounds.isEmpty()) m_bounds = computeBoundsFromPoints();
    rebuildTextAndView();
    m_gcodeView->resetView(); // только при первичной загрузке/показе
}

void GCodeWindow::rebuildTextAndView() {
    QVector<QPointF> positions;
    positions.reserve(m_points.size());
    for (const auto& p : m_points) positions.append(p.position);

    const QString gcode = m_gen.generateGCode(m_points, &m_pointMap, m_comment);
    // Сохраняем позицию прокрутки/курсор — чтобы не "центрировало" при обновлении.
    const int vScroll = m_textEdit->verticalScrollBar() ? m_textEdit->verticalScrollBar()->value() : 0;
    const int hScroll = m_textEdit->horizontalScrollBar() ? m_textEdit->horizontalScrollBar()->value() : 0;
    const int cursorPos = m_textEdit->textCursor().position();

    m_textEdit->setPlainText(gcode);

    if (m_textEdit->verticalScrollBar()) m_textEdit->verticalScrollBar()->setValue(vScroll);
    if (m_textEdit->horizontalScrollBar()) m_textEdit->horizontalScrollBar()->setValue(hScroll);
    QTextCursor c = m_textEdit->textCursor();
    c.setPosition(qBound(0, cursorPos, m_textEdit->document()->characterCount() - 1));
    m_textEdit->setTextCursor(c);

    m_gcodeView->setPoints(positions, m_bounds.isEmpty() ? computeBoundsFromPoints() : m_bounds);
    m_gcodeView->setSelectedIndices(m_selected);

    m_infoLabel->setText(QString("Points: %1 | Size: %2 x %3 mm")
        .arg(m_points.size())
        .arg(m_bounds.width(), 0, 'f', 1)
        .arg(m_bounds.height(), 0, 'f', 1));
    highlightSelectionInText();
    updateSelectionPanel();
}

void GCodeWindow::setSelection(const QVector<int>& indices, bool ensureVisibleInText) {
    QVector<int> newSel = indices;
    for (int i = newSel.size() - 1; i >= 0; --i) {
        if (newSel[i] < 0 || newSel[i] >= m_points.size()) newSel.removeAt(i);
    }
    std::sort(newSel.begin(), newSel.end());
    if (newSel == m_selected) return;

    m_selected = newSel;
    m_gcodeView->setSelectedIndices(m_selected);
    highlightSelectionInText();
    if (ensureVisibleInText && !m_selected.isEmpty()) scrollToPointInText(m_selected.front());
    updateSelectionPanel();
}

void GCodeWindow::highlightSelectionInText() {
    QList<QTextEdit::ExtraSelection> sels;
    if (!m_selected.isEmpty() && !m_pointMap.isEmpty()) {
        for (int idx : m_selected) {
            if (idx < 0 || idx >= m_pointMap.size()) continue;
            const auto& pm = m_pointMap[idx];
            if (pm.startLine < 0 || pm.endLine < 0) continue;

            QTextBlock b1 = m_textEdit->document()->findBlockByNumber(pm.startLine);
            QTextBlock b2 = m_textEdit->document()->findBlockByNumber(pm.endLine);
            if (!b1.isValid() || !b2.isValid()) continue;

            QTextCursor c(b1);
            c.setPosition(b1.position());
            c.setPosition(b2.position() + b2.length() - 1, QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection s;
            s.cursor = c;
            s.format.setBackground(QColor(80, 60, 20));
            s.format.setForeground(QColor(255, 240, 210));
            sels.push_back(s);
        }
    }
    m_textEdit->setExtraSelections(sels);
}

void GCodeWindow::scrollToPointInText(int pointIndex) {
    if (pointIndex < 0 || pointIndex >= m_pointMap.size()) return;
    const int line = (m_pointMap[pointIndex].xyLine >= 0) ? m_pointMap[pointIndex].xyLine : m_pointMap[pointIndex].startLine;
    if (line < 0) return;
    QTextBlock block = m_textEdit->document()->findBlockByNumber(line);
    if (!block.isValid()) return;
    QTextCursor c(block);
    m_textEdit->setTextCursor(c);
    m_textEdit->ensureCursorVisible();
}

QRectF GCodeWindow::computeBoundsFromPoints() const {
    if (m_points.isEmpty()) return QRectF();
    QRectF r(m_points[0].position, QSizeF(0, 0));
    for (const auto& p : m_points) r |= QRectF(p.position, QSizeF(0, 0));
    const double m = 2.0;
    return r.adjusted(-m, -m, m, m);
}

void GCodeWindow::setupViewMenu() {
    QMenu* viewMenu = menuBar()->addMenu(tr("Вид"));

    QMenu* colorMenu = viewMenu->addMenu(tr("Цвета"));
    colorMenu->addAction(tr("Фон..."), this, [this] {
        const QColor c = QColorDialog::getColor(m_gcodeView->backgroundColor(), this, tr("Фон"));
        if (c.isValid()) m_gcodeView->setBackgroundColor(c);
    });
    colorMenu->addAction(tr("Сетка..."), this, [this] {
        const QColor c = QColorDialog::getColor(m_gcodeView->gridColor(), this, tr("Сетка"));
        if (c.isValid()) m_gcodeView->setGridColor(c);
    });
    colorMenu->addAction(tr("Точки..."), this, [this] {
        const QColor c = QColorDialog::getColor(m_gcodeView->pointColor(), this, tr("Точки"));
        if (c.isValid()) m_gcodeView->setPointColor(c);
    });
    colorMenu->addAction(tr("Выделенные точки..."), this, [this] {
        const QColor c = QColorDialog::getColor(m_gcodeView->selectedPointColor(), this, tr("Выделенные точки"));
        if (c.isValid()) m_gcodeView->setSelectedPointColor(c);
    });

    QMenu* gridMenu = viewMenu->addMenu(tr("Шаг сетки"));
    QActionGroup* gridGroup = new QActionGroup(this);
    gridGroup->setExclusive(true);

    QAction* actAuto = gridMenu->addAction(tr("Авто"));
    actAuto->setCheckable(true);
    actAuto->setChecked(true);
    gridGroup->addAction(actAuto);
    connect(actAuto, &QAction::triggered, this, [this] {
        m_gcodeView->setGridAuto(true);
    });

    const QList<double> steps{ 0.5, 1.0, 2.0, 5.0, 10.0 };
    for (double step : steps) {
        QAction* act = gridMenu->addAction(tr("%1 мм").arg(step));
        act->setCheckable(true);
        gridGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, step] {
            m_gcodeView->setFixedGridStep(step);
        });
    }

    viewMenu->addAction(m_actMeasureTape);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actShowSelectionDock);
}

void GCodeWindow::updateSelectionPanel() {
    if (!m_selectionTable || !m_selectionDock || !m_selectionDock->isVisible())
        return;
    m_selectionTable->setRowCount(0);
    for (int idx : m_selected) {
        if (idx < 0 || idx >= m_points.size()) continue;
        const DispensePoint& p = m_points[idx];
        const int row = m_selectionTable->rowCount();
        m_selectionTable->insertRow(row);
        m_selectionTable->setItem(row, 0, new QTableWidgetItem(QString::number(idx + 1)));
        m_selectionTable->setItem(row, 1, new QTableWidgetItem(QString::number(p.position.x(), 'f', 3)));
        m_selectionTable->setItem(row, 2, new QTableWidgetItem(QString::number(p.position.y(), 'f', 3)));
        m_selectionTable->setItem(row, 3, new QTableWidgetItem(QString::number(p.diameter, 'f', 3)));
        m_selectionTable->setItem(row, 4, new QTableWidgetItem(QString::number(p.height, 'f', 3)));
        m_selectionTable->setItem(row, 5, new QTableWidgetItem(QString::number(static_cast<int>(p.dispenseTime))));
    }
    m_selectionTable->resizeRowsToContents();
}

void GCodeWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (!m_selectionDock || !m_selectionDock->isVisible() || m_selectionDock->isFloating())
        return;
    const int minDockW = m_selectionDock->minimumWidth();
    if (m_selectionDock->width() < minDockW - 2) {
        const int target = qBound(minDockW, width() / 3, qMax(minDockW, 520));
        if (width() >= target + 350)
            resizeDocks({ m_selectionDock }, { target }, Qt::Horizontal);
    }
}
