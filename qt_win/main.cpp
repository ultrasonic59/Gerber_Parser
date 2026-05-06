#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Gerber Parser");
    app.setApplicationVersion("1.0.0");

    app.setStyleSheet(R"(
        QMainWindow { background-color: #1e1e1e; }
        QGroupBox {
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
            margin-top: 10px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QTreeWidget, QTableWidget {
            background-color: #252526;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            gridline-color: #3c3c3c;
            alternate-background-color: #2d2d2d;
        }
        QHeaderView::section {
            background-color: #333;
            color: #d4d4d4;
            border: 1px solid #3c3c3c;
            padding: 4px;
        }
        QTreeWidget::item:selected, QTableWidget::item:selected {
            background-color: #094771;
        }
        QMenuBar { background-color: #2d2d2d; color: #d4d4d4; }
        QMenuBar::item:selected { background-color: #094771; }
        QMenu { background-color: #2d2d2d; color: #d4d4d4; border: 1px solid #3c3c3c; }
        QMenu::item:selected { background-color: #094771; }
        QStatusBar { background-color: #1e1e1e; color: #999; }
        QProgressBar {
            background-color: #3c3c3c; color: white; border: none;
            border-radius: 4px; text-align: center;
        }
        QProgressBar::chunk { background-color: #0e7ad1; border-radius: 4px; }
        QSplitter::handle { background-color: #3c3c3c; width: 2px; }
    )");

    MainWindow window;
    window.show();
    return app.exec();
}
