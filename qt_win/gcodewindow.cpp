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
#include <cmath>
#include <QScrollBar>

// ==================== GCodeView ====================

GCodeView::GCodeView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 250);
    setMouseTracking(true);
}

void GCodeView::setPoints(const QVector<QPointF>& points, const QRectF& bounds) {
    m_points = points;
    m_bounds = bounds;
    m_selectedIndices.clear();
    resetView();
}

void GCodeView::resetView() {
    m_scale = 1.0;
    m_offsetX = 0.0;
    m_offsetY = 0.0;
    update();
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

int GCodeView::findNearestPoint(const QPointF& worldPos, double maxDist) const {
    int nearest = -1;
    double minDist = maxDist;

    for (int i = 0; i < m_points.size(); ++i) {
        QPointF sp = worldToScreen(m_points[i]);
        QPointF wp = worldToScreen(worldPos);
        double dx = sp.x() - wp.x();
        double dy = sp.y() - wp.y();
        double dist = sqrt(dx * dx + dy * dy);
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
    if (event->button() == Qt::LeftButton) {
        QPointF worldPos = screenToWorld(QPointF(event->pos().x(), event->pos().y()));

        // Ищем ближайшую точку (в пикселях экрана)
        int nearest = findNearestPoint(worldPos, 15.0);

        if (nearest >= 0) {
            // Ctrl+Click — добавляем/убираем точку
            if (event->modifiers() & Qt::ControlModifier) {
                if (m_selectedIndices.contains(nearest)) {
                    m_selectedIndices.removeAll(nearest);
                }
                else {
                    m_selectedIndices.append(nearest);
                }
                update();
                emit selectionChanged(m_selectedIndices);
                return;
            }
            // Обычный клик — выбираем только одну точку
            m_selectedIndices.clear();
            m_selectedIndices.append(nearest);
            update();
            emit selectionChanged(m_selectedIndices);
            return;
        }

        // Клик мимо точек — начинаем панорамирование
        if (!(event->modifiers() & Qt::ControlModifier)) {
            m_dragging = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }
    else if (event->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void GCodeView::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offsetX += delta.x();
        m_offsetY += delta.y();
        m_lastMousePos = event->pos();
        update();
    }
    else {
        QPointF world = screenToWorld(QPointF(event->pos().x(), event->pos().y()));
        // Ищем точку под курсором
        int hovered = findNearestPoint(world, 12.0);
        if (hovered >= 0) {
            setToolTip(QString("Point %1\nX: %2 mm\nY: %3 mm\nCtrl+Click to add/remove")
                .arg(hovered + 1)
                .arg(m_points[hovered].x(), 0, 'f', 3)
                .arg(m_points[hovered].y(), 0, 'f', 3));
            setCursor(Qt::PointingHandCursor);
        }
        else {
            setToolTip(QString("X: %1 mm\nY: %2 mm")
                .arg(world.x(), 0, 'f', 3)
                .arg(world.y(), 0, 'f', 3));
            setCursor(Qt::ArrowCursor);
        }
    }
}

void GCodeView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
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

    // Маршрут (все точки — серый пунктир)
    painter.setPen(QPen(QColor(0, 180, 100, 80), 1.0, Qt::DashLine));
    for (int i = 0; i < m_points.size() - 1; ++i) {
        painter.drawLine(worldToScreen(m_points[i]), worldToScreen(m_points[i + 1]));
    }

    // Точки дозирования
    double pointScale = qMin(scale * 0.3, 8.0);
    if (pointScale < 1.5) pointScale = 1.5;

    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF& pt = m_points[i];
        QPointF sp = worldToScreen(pt);

        if (sp.x() < -10 || sp.x() > width() + 10 ||
            sp.y() < -10 || sp.y() > height() + 10) continue;

        if (m_selectedIndices.contains(i)) {
            // ВЫДЕЛЕННАЯ ТОЧКА
            painter.setBrush(QColor(50, 255, 80));     // Ярко-зелёная заливка
            painter.setPen(QPen(QColor(200, 255, 200), 2.0));
            painter.drawEllipse(sp, pointScale + 3, pointScale + 3);

            // Контур
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255), 1.0, Qt::DotLine));
            painter.drawEllipse(sp, pointScale + 6, pointScale + 6);

            // Номер точки
            painter.setPen(QColor(200, 255, 200));
            QFont f = painter.font(); f.setPointSize(7); painter.setFont(f);
            painter.drawText(sp + QPointF(6, -6), QString::number(i + 1));
        }
        else {
            // ОБЫЧНАЯ ТОЧКА
            painter.setBrush(QColor(255, 200, 50));
            painter.setPen(QPen(QColor(200, 150, 0), 1.0));
            painter.drawEllipse(sp, pointScale, pointScale);
        }
    }

    // Информация
    painter.setPen(QColor(120, 130, 140));
    QFont f = painter.font(); f.setPointSize(9); painter.setFont(f);

    QString selInfo;
    if (m_selectedIndices.isEmpty()) {
        selInfo = "Click to select | Ctrl+Click for multi";
    }
    else {
        selInfo = QString("Selected: %1 points").arg(m_selectedIndices.size());
    }

    painter.drawText(5, height() - 18,
        QString("Points: %1 | Scale: %2x | Grid: %3 mm")
        .arg(m_points.size())
        .arg(m_scale, 0, 'f', 2)
        .arg(gridStep, 0, 'f', 1));
    painter.drawText(5, height() - 5, selInfo);
    painter.drawText(width() - 200, height() - 5,
        QString("Bounds: %1 x %2 mm")
        .arg(m_bounds.width(), 0, 'f', 1)
        .arg(m_bounds.height(), 0, 'f', 1));
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

    // Сплиттер: графика слева, код справа
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Графический вид (слева)
    m_gcodeView = new GCodeView(this);
    m_splitter->addWidget(m_gcodeView);

    // Текстовое поле (справа)
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

    // Панель кнопок
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setContentsMargins(0, 4, 0, 0);

    m_infoLabel = new QLabel("Ready");
    m_infoLabel->setStyleSheet("color: #999;");
    btnLayout->addWidget(m_infoLabel);

    btnLayout->addStretch();

    m_resetViewBtn = new QPushButton("🔄 Reset View", this);
    connect(m_resetViewBtn, &QPushButton::clicked, this, [this]() {
        m_gcodeView->resetView();
        });
    btnLayout->addWidget(m_resetViewBtn);

    m_copyBtn = new QPushButton("📋 Copy G-Code", this);
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_textEdit->toPlainText());
        m_infoLabel->setText("✅ Copied!");
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
            m_infoLabel->setText("✅ Saved!");
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
