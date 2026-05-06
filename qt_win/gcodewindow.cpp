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
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <cmath>
#include <QFileInfo>

// ==================== GCodeView ====================

GCodeView::GCodeView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 250);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void GCodeView::setPoints(const QVector<QPointF>& points, const QRectF& bounds) {
    m_points = points;
    m_bounds = bounds;
    m_selectedPointIndex = -1;
    m_draggedPointIndex = -1;
    resetView();
}

void GCodeView::resetView() {
    m_scale = 1.0;
    m_offsetX = 0.0;
    m_offsetY = 0.0;
    update();
}

void GCodeView::deleteSelectedPoint() {
    if (m_selectedPointIndex >= 0 && m_selectedPointIndex < m_points.size()) {
        m_points.removeAt(m_selectedPointIndex);
        m_selectedPointIndex = -1;
        emit pointsChanged();
        update();
    }
}

void GCodeView::moveSelectedPoint(const QPointF& newPos) {
    if (m_selectedPointIndex >= 0 && m_selectedPointIndex < m_points.size()) {
        m_points[m_selectedPointIndex] = newPos;
        emit pointsChanged();
        update();
    }
}

QPointF GCodeView::worldToScreen(const QPointF& world) const {
    if (m_bounds.isEmpty()) return QPointF();
    double scale = qMin((width() - 20) / m_bounds.width(),
        (height() - 20) / m_bounds.height()) * m_scale;
    double ox = 10 - m_bounds.left() * scale + m_offsetX;
    double oy = 10 - m_bounds.top() * scale + m_offsetY;
    return QPointF(world.x() * scale + ox, world.y() * scale + oy);
}

QPointF GCodeView::screenToWorld(const QPointF& screen) const {
    if (m_bounds.isEmpty()) return QPointF();
    double scale = qMin((width() - 20) / m_bounds.width(),
        (height() - 20) / m_bounds.height()) * m_scale;
    double ox = 10 - m_bounds.left() * scale + m_offsetX;
    double oy = 10 - m_bounds.top() * scale + m_offsetY;
    return QPointF((screen.x() - ox) / scale, (screen.y() - oy) / scale);
}

int GCodeView::findNearestPoint(const QPointF& screenPos, double maxDist) const {
    int nearest = -1;
    double minDist = maxDist;
    for (int i = 0; i < m_points.size(); ++i) {
        QPointF sp = worldToScreen(m_points[i]);
        double dist = sqrt(pow(sp.x() - screenPos.x(), 2) + pow(sp.y() - screenPos.y(), 2));
        if (dist < minDist) {
            minDist = dist;
            nearest = i;
        }
    }
    return nearest;
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

    if (event->button() == Qt::LeftButton) {
        // Проверяем, кликнули ли на точку
        int idx = findNearestPoint(QPointF(event->pos().x(), event->pos().y()));
        if (idx >= 0) {
            // Начинаем перетаскивание точки
            m_selectedPointIndex = idx;
            m_draggingPoint = true;
            m_draggedPointIndex = idx;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            update();
        }
        else {
            // Снимаем выделение
            m_selectedPointIndex = -1;
            // Начинаем панорамирование
            m_dragging = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            update();
        }
    }
    else if (event->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    else if (event->button() == Qt::RightButton) {
        // Выбираем точку под курсором для контекстного меню
        int idx = findNearestPoint(QPointF(event->pos().x(), event->pos().y()));
        if (idx >= 0) {
            m_selectedPointIndex = idx;
            update();
        }
    }
}

void GCodeView::mouseMoveEvent(QMouseEvent* event) {
    QPointF world = screenToWorld(QPointF(event->pos().x(), event->pos().y()));

    if (m_draggingPoint && m_draggedPointIndex >= 0) {
        // Перемещаем точку
        m_points[m_draggedPointIndex] = world;
        update();
    }
    else if (m_dragging) {
        // Панорамирование
        QPoint delta = event->pos() - m_lastMousePos;
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        m_lastMousePos = event->pos();
        update();
    }
    else {
        // Подсветка точки под курсором
        int idx = findNearestPoint(QPointF(event->pos().x(), event->pos().y()));
        if (idx >= 0) {
            setCursor(Qt::PointingHandCursor);
            setToolTip(QString("Point %1\nX: %2 mm\nY: %3 mm\nClick to select, drag to move")
                .arg(idx + 1)
                .arg(m_points[idx].x(), 0, 'f', 3)
                .arg(m_points[idx].y(), 0, 'f', 3));
        }
        else {
            setCursor(Qt::ArrowCursor);
            setToolTip(QString("X: %1 mm\nY: %2 mm\nScroll to zoom, drag to pan")
                .arg(world.x(), 0, 'f', 3)
                .arg(world.y(), 0, 'f', 3));
        }
    }
}

void GCodeView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_draggingPoint && m_draggedPointIndex >= 0) {
            // Закончили перемещение точки
            emit pointsChanged();
        }
        m_dragging = false;
        m_draggingPoint = false;
        m_draggedPointIndex = -1;
        setCursor(Qt::ArrowCursor);
    }
    else if (event->button() == Qt::MiddleButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void GCodeView::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);

    if (m_selectedPointIndex >= 0) {
        QAction* deleteAction = menu.addAction(QIcon(), "🗑 Delete Point");
        connect(deleteAction, &QAction::triggered, this, [this]() {
            deleteSelectedPoint();
            });

        QAction* moveToOrigin = menu.addAction(QIcon(), "📍 Move to (0,0)");
        connect(moveToOrigin, &QAction::triggered, this, [this]() {
            moveSelectedPoint(QPointF(0, 0));
            });

        menu.addSeparator();
    }

    QAction* selectAllAction = menu.addAction(QIcon(), "🔵 Deselect All");
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        m_selectedPointIndex = -1;
        update();
        });

    menu.addSeparator();

    QAction* resetAction = menu.addAction(QIcon(), "🔄 Reset View");
    connect(resetAction, &QAction::triggered, this, [this]() {
        resetView();
        });

    menu.exec(event->globalPos());
}

void GCodeView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelectedPoint();
    }
    else if (event->key() == Qt::Key_Escape) {
        m_selectedPointIndex = -1;
        update();
    }
}

void GCodeView::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Фон
    painter.fillRect(rect(), QColor(18, 22, 28));

    if (m_points.isEmpty() || m_bounds.isEmpty()) {
        painter.setPen(QColor(100, 100, 100));
        QFont f = painter.font(); f.setPointSize(14); painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, "No dispense points");
        return;
    }

    // Сетка
    double gridStep = 5.0;
    double scale = qMin((width() - 20) / m_bounds.width(),
        (height() - 20) / m_bounds.height()) * m_scale;
    while (gridStep * scale < 20) gridStep *= 2;
    while (gridStep * scale > 80) gridStep /= 2;

    painter.setPen(QPen(QColor(35, 42, 50), 0.5));
    for (double x = floor(m_bounds.left() / gridStep) * gridStep;
        x < m_bounds.right(); x += gridStep) {
        QPointF p1 = worldToScreen(QPointF(x, m_bounds.top()));
        QPointF p2 = worldToScreen(QPointF(x, m_bounds.bottom()));
        painter.drawLine(p1, p2);
    }
    for (double y = floor(m_bounds.top() / gridStep) * gridStep;
        y < m_bounds.bottom(); y += gridStep) {
        QPointF p1 = worldToScreen(QPointF(m_bounds.left(), y));
        QPointF p2 = worldToScreen(QPointF(m_bounds.right(), y));
        painter.drawLine(p1, p2);
    }

    // Маршрут
    painter.setPen(QPen(QColor(0, 180, 100, 80), 1.0, Qt::DashLine));
    for (int i = 0; i < m_points.size() - 1; ++i) {
        painter.drawLine(worldToScreen(m_points[i]), worldToScreen(m_points[i + 1]));
    }

    // Точки дозирования
    double pointScale = qMin(scale * 0.3, 8.0);
    if (pointScale < 1.5) pointScale = 1.5;

    for (int i = 0; i < m_points.size(); ++i) {
        QPointF sp = worldToScreen(m_points[i]);
        if (sp.x() < -10 || sp.x() > width() + 10 ||
            sp.y() < -10 || sp.y() > height() + 10)
            continue;

        if (i == m_selectedPointIndex) {
            // Выделенная точка — красная
            painter.setBrush(QColor(255, 80, 80));
            painter.setPen(QPen(QColor(255, 255, 255), 2.0));
            painter.drawEllipse(sp, pointScale + 2, pointScale + 2);

            // Номер точки
            painter.setPen(Qt::white);
            QFont f = painter.font(); f.setPointSize(8); painter.setFont(f);
            painter.drawText(sp + QPointF(6, -6), QString("#%1").arg(i + 1));
        }
        else {
            // Обычная точка — жёлтая
            painter.setBrush(QColor(255, 200, 50));
            painter.setPen(QPen(QColor(200, 150, 0), 1.0));
            painter.drawEllipse(sp, pointScale, pointScale);
        }
    }

    // Информация
    painter.setPen(QColor(120, 130, 140));
    QFont f = painter.font(); f.setPointSize(9); painter.setFont(f);

    QString info = QString("Points: %1 | Sel: %2 | Scale: %3x | Grid: %4 mm | Bounds: %5×%6 mm")
        .arg(m_points.size())
        .arg(m_selectedPointIndex >= 0 ? QString::number(m_selectedPointIndex + 1) : "-")
        .arg(m_scale, 0, 'f', 2)
        .arg(gridStep, 0, 'f', 1)
        .arg(m_bounds.width(), 0, 'f', 1)
        .arg(m_bounds.height(), 0, 'f', 1);

    painter.drawText(5, height() - 15, info);
    painter.drawText(5, height() - 4, "🖱 Click=select | Drag=move | Del=delete | RClick=menu | Scroll=zoom");
}

// ==================== GCodeWindow ====================

GCodeWindow::GCodeWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("G-Code Generator — Paste Dispenser");
    setMinimumSize(750, 550);
    resize(900, 650);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Сплиттер
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_gcodeView = new GCodeView(this);
    m_splitter->addWidget(m_gcodeView);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Consolas", 9));
    m_textEdit->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: none;");
    m_textEdit->setMinimumWidth(250);
    m_splitter->addWidget(m_textEdit);

    m_splitter->setSizes({ 500, 350 });
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    mainLayout->addWidget(m_splitter);

    // Сигнал изменения точек
    connect(m_gcodeView, &GCodeView::pointsChanged, this, [this]() {
        QVector<QPointF> pts = m_gcodeView->points();
        QString gcode = regenerateGCode(pts);
        m_textEdit->setPlainText(gcode);
        m_infoLabel->setText(QString("📌 Points: %1 (edited)").arg(pts.size()));
        });

    // Панель кнопок
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 4, 0, 0);

    m_infoLabel = new QLabel("Ready");
    m_infoLabel->setStyleSheet("color: #999;");
    btnLayout->addWidget(m_infoLabel);

    btnLayout->addStretch();

    m_deletePointBtn = new QPushButton("🗑 Delete Point", this);
    m_deletePointBtn->setToolTip("Delete selected point (Del key)");
    connect(m_deletePointBtn, &QPushButton::clicked, this, [this]() {
        m_gcodeView->deleteSelectedPoint();
        });
    btnLayout->addWidget(m_deletePointBtn);

    m_resetViewBtn = new QPushButton("🔄 Reset View", this);
    connect(m_resetViewBtn, &QPushButton::clicked, this, [this]() {
        m_gcodeView->resetView();
        });
    btnLayout->addWidget(m_resetViewBtn);

    m_copyBtn = new QPushButton("📋 Copy G-Code", this);
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_textEdit->toPlainText());
        m_infoLabel->setText("✅ Copied to clipboard!");
        });
    btnLayout->addWidget(m_copyBtn);

    m_saveBtn = new QPushButton("💾 Save .gcode", this);
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
            m_infoLabel->setText("✅ Saved: " + QFileInfo(path).fileName());
        }
        });
    btnLayout->addWidget(m_saveBtn);

    mainLayout->addLayout(btnLayout);
}

void GCodeWindow::setGCode(const QString& gcode, const QVector<QPointF>& points, const QRectF& bounds) {
    m_textEdit->setPlainText(gcode);
    m_gcodeView->setPoints(points, bounds);
    m_infoLabel->setText(QString("📌 Points: %1 | Size: %2 × %3 mm")
        .arg(points.size())
        .arg(bounds.width(), 0, 'f', 1)
        .arg(bounds.height(), 0, 'f', 1));
}

QString GCodeWindow::regenerateGCode(const QVector<QPointF>& points) {
    QString result;
    QTextStream out(&result);

    out << "; ===== G-CODE FOR PASTE DISPENSER (EDITED) =====\n";
    out << "; Generated by GerberParser\n";
    out << "; Points: " << points.size() << " (manually edited)\n";
    out << "; \n";
    out << "G21 ; Units in mm\n";
    out << "G90 ; Absolute positioning\n";
    out << "G00 Z5.0 ; Raise Z\n";
    out << "G00 X0 Y0 ; Go to origin\n";
    out << "M03 S100 ; Start pump\n";
    out << "G04 P500 ; Wait for pump\n";
    out << "\n";

    for (int i = 0; i < points.size(); ++i) {
        const QPointF& pt = points[i];
        out << "; Point " << (i + 1) << "\n";
        out << "G00 Z5.0\n";
        out << "G00 X" << QString::number(pt.x(), 'f', 3)
            << " Y" << QString::number(pt.y(), 'f', 3) << "\n";
        out << "G01 Z0.2 F1500\n";
        out << "G04 P200\n";
        out << "G00 Z5.0\n";
    }

    out << "\n";
    out << "M05 ; Stop pump\n";
    out << "G00 X0 Y0 ; Return to origin\n";
    out << "G00 Z5.0\n";
    out << "M02 ; End of program\n";

    return result;
}
