#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTimer>
#include "Gerber_Parser.h"
#include "gerberview.h"
#include "gerbertexteditor.h"
#include "gerbersourcemap.h"
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
    void onSaveFile();
    void onCloseAll();
    void onAbout();
    void onParseProgress(int percent);
    void onLayerSelected();
    void onGenerateGCode();
    void onTextEditFinished();
    void onViewLayerEdited(const GerberLayer& layer);
    void onViewSelectionChanged(int polyIndex, int pointIndex);
    void onTextCursorObject(int polyIndex, int pointIndex);

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void loadFile(const QString &filePath);
    void updateLayerList();
    void refreshCurrentLayerUi(bool keepViewTransform = false);
    void applyLayerToViewAndText(const GerberLayer& layer, bool updateText, bool keepViewTransform);
    void syncTextFromLayer();
    void rebuildSourceMap();
    void updateTextHighlightFromView(int polyIndex, int pointIndex);
    int currentLayerIndex() const;

    Gerber_Parser       *m_parser;
    GerberView          *m_view;
    GerberTextEditor    *m_textEditor;
    QTreeWidget         *m_layerTree;
    QTableWidget        *m_infoTable;
    QLabel              *m_statusLabel;
    QProgressBar        *m_progressBar;
    QSplitter           *m_splitter;
    QSplitter           *m_rightSplitter;
    QTimer              *m_textParseTimer;

    QVector<GerberLayer> m_layers;
    GerberSourceMap m_sourceMap;
    bool m_syncBlock = false;

    GCodeGenerator* m_gcodeGen;
    GCodeWindow* m_gcodeWindow;
};

#endif // MAINWINDOW_H
