#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include "Gerber_Parser.h"
#include "gerberview.h"
#include "gcodegenerator.h"
#include "gcodewindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onOpenFile();
    void onOpenMultiple();
    void onCloseAll();
    void onAbout();
    void onParseProgress(int percent);
    void onLayerSelected();
    void onGenerateGCode();
private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void loadFile(const QString &filePath);
    void updateLayerList();

    Gerber_Parser    *m_parser;
    GerberView      *m_view;
    QTreeWidget     *m_layerTree;
    QTableWidget    *m_infoTable;
    QLabel          *m_statusLabel;
    QProgressBar    *m_progressBar;
    QSplitter       *m_splitter;

    QVector<GerberLayer> m_layers;
    int m_currentLayerIndex = -1;
private:
    GCodeGenerator* m_gcodeGen;
    GCodeWindow* m_gcodeWindow;

};

#endif // MAINWINDOW_H
