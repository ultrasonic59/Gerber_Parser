#include "mainwindow.h"
#include "gerberwriter.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QApplication>
#include <QDebug>
#include <QToolBar>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_parser = new Gerber_Parser(this);
    setupUI();
    setupMenuBar();
    setupStatusBar();

    QToolBar* tb = addToolBar("GCode");
    QAction* gcodeAct = tb->addAction("⚙ Generate G-Code");

    gcodeAct->setToolTip("Generate G-Code for paste dispenser");
    connect(gcodeAct, &QAction::triggered, this, &MainWindow::onGenerateGCode);

    connect(m_parser, &Gerber_Parser::parseProgress, this, &MainWindow::onParseProgress);
    connect(m_layerTree, &QTreeWidget::currentItemChanged, this, [this]() { onLayerSelected(); });
    connect(m_view, &GerberView::layerEdited, this, &MainWindow::onViewLayerEdited);
    connect(m_view, &GerberView::selectionChanged, this, &MainWindow::onViewSelectionChanged);
    connect(m_textEditor, &GerberTextEditor::objectAtCursor, this, &MainWindow::onTextCursorObject);

    m_textParseTimer = new QTimer(this);
    m_textParseTimer->setSingleShot(true);
    m_textParseTimer->setInterval(450);
    connect(m_textEditor, &GerberTextEditor::editFinished, m_textParseTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_textParseTimer, &QTimer::timeout, this, &MainWindow::onTextEditFinished);

    setWindowTitle("Gerber Parser — PCB Editor");
    resize(1200, 800);
    m_gcodeGen = new GCodeGenerator();
    m_gcodeWindow = nullptr;

}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    m_splitter = new QSplitter(Qt::Horizontal, central);

    // Левая панель
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);

    QGroupBox *layersGroup = new QGroupBox("Gerber Layers");
    QVBoxLayout *layersLayout = new QVBoxLayout(layersGroup);
    m_layerTree = new QTreeWidget();
    m_layerTree->setHeaderLabels({"Layer", "Polys", "Segs", "Size"});
    m_layerTree->setAlternatingRowColors(true);
    m_layerTree->setRootIsDecorated(false);
    layersLayout->addWidget(m_layerTree);
    leftLayout->addWidget(layersGroup);

    QGroupBox *infoGroup = new QGroupBox("Layer Info");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
    m_infoTable = new QTableWidget(0, 2);
    m_infoTable->setHorizontalHeaderLabels({"Property", "Value"});
    m_infoTable->horizontalHeader()->setStretchLastSection(true);
    m_infoTable->setAlternatingRowColors(true);
    m_infoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    infoLayout->addWidget(m_infoTable);
    leftLayout->addWidget(infoGroup);

    m_rightSplitter = new QSplitter(Qt::Vertical);
    m_view = new GerberView();
    m_textEditor = new GerberTextEditor();

    QGroupBox* textGroup = new QGroupBox("Gerber Source");
    QVBoxLayout* textLayout = new QVBoxLayout(textGroup);
    textLayout->addWidget(m_textEditor);

    m_rightSplitter->addWidget(m_view);
    m_rightSplitter->addWidget(textGroup);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 2);

    m_splitter->addWidget(leftPanel);
    m_splitter->addWidget(m_rightSplitter);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 4);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->addWidget(m_splitter);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *openAct = fileMenu->addAction("&Open Gerber...", this, &MainWindow::onOpenFile);
    openAct->setShortcut(QKeySequence::Open);

    QAction *openMulAct = fileMenu->addAction("Open &Multiple...", this, &MainWindow::onOpenMultiple);
    openMulAct->setShortcut(QKeySequence("Ctrl+Shift+O"));

    fileMenu->addAction("&Save Gerber...", this, &MainWindow::onSaveFile, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("&Close All", this, &MainWindow::onCloseAll, QKeySequence("Ctrl+W"));
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", qApp, &QApplication::quit, QKeySequence::Quit);

    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("&Fit in View", m_view, &GerberView::fitInView, QKeySequence("Ctrl+F"));

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel, 1);

    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Gerber File", QString(),
        "Gerber Files (*.gbp *.gtp *.gbr *.ger *.pho *.cmp *.sol *.top *.bot *.gko *.gtl *.gbl *.gts *.gbs *.gto *.gbo *.gpt *.gpb);;All Files (*.*)");

    if (!filePath.isEmpty())
        loadFile(filePath);
}

void MainWindow::onOpenMultiple()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Open Gerber Files", QString(),
        "Gerber Files (*.gbp *.gtp *.gbr *.ger *.pho *.cmp *.sol *.top *.bot *.gko *.gtl *.gbl *.gts *.gbs *.gto *.gbo *.gpt *.gpb);;All Files (*.*)");

    for (const QString &file : files)
        loadFile(file);
}

void MainWindow::onSaveFile()
{
    const int idx = currentLayerIndex();
    if (idx < 0)
        return;

    GerberLayer& layer = m_layers[idx];
    QString path = layer.sourcePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Gerber File", layer.fileName,
            "Gerber Files (*.gbr *.ger *.pho);;All Files (*.*)");
        if (path.isEmpty())
            return;
        layer.sourcePath = path;
        layer.fileName = QFileInfo(path).fileName();
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save", "Cannot write file:\n" + path);
        return;
    }

    const QString text = m_textEditor->toPlainText();
    file.write(text.toUtf8());
    file.close();
    layer.sourceText = text;
    m_statusLabel->setText(QString("Saved: %1").arg(layer.fileName));
}

void MainWindow::onCloseAll()
{
    m_layers.clear();
    m_layerTree->clear();
    m_infoTable->setRowCount(0);
    m_view->clear();
    m_textEditor->clear();
    m_statusLabel->setText("Closed all layers");
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "About Gerber Parser",
        "<h2>Gerber Parser v1.0</h2>"
        "<p>PCB Gerber file viewer (RS-274X).</p>"
        "<p>Built with Qt 5.15 + C++17.</p>"
        "<p>© 2026 MashGPT</p>");
}

void MainWindow::onParseProgress(int percent)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(QString("Parsing... %1%").arg(percent));
}

int MainWindow::currentLayerIndex() const
{
    return m_layerTree->currentIndex().row();
}

void MainWindow::rebuildSourceMap() {
    const int idx = currentLayerIndex();
    if (idx < 0 || idx >= m_layers.size())
        return;

    const GerberLayer& layer = m_layers[idx];
    const QString text = m_textEditor->toPlainText();
    m_sourceMap.rebuild(text, layer);
    m_textEditor->setSourceContext(&m_sourceMap, &m_layers[idx]);
}

void MainWindow::updateTextHighlightFromView(int polyIndex, int pointIndex) {
    if (polyIndex < 0) {
        m_textEditor->clearObjectHighlight();
        return;
    }
    if (pointIndex < 0)
        m_textEditor->highlightPolygon(polyIndex);
    else
        m_textEditor->highlightVertex(polyIndex, pointIndex);
}

void MainWindow::onViewSelectionChanged(int polyIndex, int pointIndex) {
    if (m_syncBlock)
        return;

    const int idx = currentLayerIndex();
    if (idx >= 0 && idx < m_layers.size())
        m_layers[idx] = m_view->layer();

    rebuildSourceMap();
    updateTextHighlightFromView(polyIndex, pointIndex);
}

void MainWindow::onTextCursorObject(int polyIndex, int pointIndex) {
    if (m_syncBlock || polyIndex < 0)
        return;

    m_syncBlock = true;
    m_view->setSelectedVertex(polyIndex, pointIndex);
    if (pointIndex >= 0)
        m_textEditor->highlightVertex(polyIndex, pointIndex);
    else
        m_textEditor->highlightPolygon(polyIndex);
    m_syncBlock = false;
}

void MainWindow::applyLayerToViewAndText(const GerberLayer& layer, bool updateText, bool keepViewTransform)
{
    m_syncBlock = true;
    m_view->setLayer(layer, keepViewTransform);
    if (updateText) {
        const QString text = layer.sourceText.isEmpty()
            ? GerberWriter::layerToText(layer)
            : layer.sourceText;
        m_textEditor->setPlainTextSilent(text);
    }
    m_syncBlock = false;
    rebuildSourceMap();
}

void MainWindow::syncTextFromLayer()
{
    const int idx = currentLayerIndex();
    if (idx < 0 || idx >= m_layers.size())
        return;

    m_syncBlock = true;
    m_layers[idx].sourceText = GerberWriter::layerToText(m_layers[idx]);
    m_textEditor->setPlainTextSilent(m_layers[idx].sourceText);
    m_syncBlock = false;
}

void MainWindow::refreshCurrentLayerUi(bool keepViewTransform)
{
    const int idx = currentLayerIndex();
    if (idx < 0 || idx >= m_layers.size())
        return;

    const GerberLayer& layer = m_layers[idx];
    applyLayerToViewAndText(layer, true, keepViewTransform);

    m_infoTable->setRowCount(0);
    auto addInfo = [this](const QString &p, const QString &v) {
        int r = m_infoTable->rowCount();
        m_infoTable->insertRow(r);
        m_infoTable->setItem(r, 0, new QTableWidgetItem(p));
        m_infoTable->setItem(r, 1, new QTableWidgetItem(v));
    };

    addInfo("File", layer.fileName);
    addInfo("Status", layer.valid ? "✓ Valid" : "✗ Error");
    addInfo("Units", layer.units == GerberUnit::Millimeters ? "mm" : "inches");
    addInfo("Polygons", QString::number(layer.polygons.size()));
    addInfo("Segments", QString::number(layer.segments.size()));
    addInfo("Commands", QString::number(layer.commands.size()));

    if (!layer.boundingRect.isEmpty()) {
        addInfo("Width", QString::number(layer.boundingRect.width(), 'f', 3) + " mm");
        addInfo("Height", QString::number(layer.boundingRect.height(), 'f', 3) + " mm");
    }
    if (!layer.errorMessage.isEmpty())
        addInfo("Errors", layer.errorMessage);

    m_statusLabel->setText(QString("Layer: %1 | %2 polys, %3 segs")
        .arg(layer.fileName).arg(layer.polygons.size()).arg(layer.segments.size()));
}

void MainWindow::onTextEditFinished()
{
    if (m_syncBlock)
        return;

    const int idx = currentLayerIndex();
    if (idx < 0 || idx >= m_layers.size())
        return;

    m_statusLabel->setText("Parsing text...");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    const QString text = m_textEditor->toPlainText();
    GerberParseOptions opts;
    opts.mergePolygons = true;

    GerberLayer layer = m_parser->parseText(text, m_layers[idx].fileName, opts);
    layer.sourcePath = m_layers[idx].sourcePath;
    layer.sourceText = text;

    m_progressBar->setVisible(false);

    if (!layer.valid && layer.polygons.isEmpty() && layer.segments.isEmpty()) {
        m_statusLabel->setText("Text parse error: " + m_parser->lastError());
        return;
    }

    m_layers[idx] = layer;
    updateLayerList();
    m_syncBlock = true;
    m_view->setLayer(layer, true);
    m_syncBlock = false;
    refreshCurrentLayerUi(true);
    rebuildSourceMap();
    const auto sel = m_view->selectedVertex();
    updateTextHighlightFromView(sel.polyIndex, sel.hasVertex() ? sel.pointIndex : -1);
    m_statusLabel->setText(QString("Text synced: %1").arg(layer.fileName));
}

void MainWindow::onViewLayerEdited(const GerberLayer& layer)
{
    if (m_syncBlock)
        return;

    const int idx = currentLayerIndex();
    if (idx < 0)
        return;

    m_layers[idx] = layer;
    m_layers[idx].sourceText = GerberWriter::layerToText(layer);

    m_syncBlock = true;
    m_textEditor->setPlainTextSilent(m_layers[idx].sourceText);
    m_view->setLayer(m_layers[idx], true);
    m_syncBlock = false;

    rebuildSourceMap();
    updateLayerList();
    updateTextHighlightFromView(m_view->selectedVertex().polyIndex,
        m_view->selectedVertex().pointIndex);
    m_statusLabel->setText(QString("Geometry synced: %1").arg(layer.fileName));
}

void MainWindow::onLayerSelected()
{
    refreshCurrentLayerUi(false);
}

void MainWindow::loadFile(const QString &filePath)
{
    m_statusLabel->setText(QString("Loading %1...").arg(QFileInfo(filePath).fileName()));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    GerberParseOptions opts;
    opts.debugOutput = false;
    opts.mergePolygons = true;

    GerberLayer layer = m_parser->parseFile(filePath, opts);
    m_progressBar->setVisible(false);

    if (!layer.valid && layer.polygons.isEmpty() && layer.segments.isEmpty()) {
        QMessageBox::warning(this, "Error", "Failed to parse:\n" + filePath + "\n\n" + m_parser->lastError());
        m_statusLabel->setText("Parse failed");
        return;
    }

    QFile src(filePath);
    if (src.open(QIODevice::ReadOnly | QIODevice::Text))
        layer.sourceText = QString::fromUtf8(src.readAll());

    m_layers.append(layer);
    updateLayerList();
    m_layerTree->setCurrentIndex(m_layerTree->model()->index(m_layers.size() - 1, 0));
    onLayerSelected();
    m_statusLabel->setText(QString("Loaded: %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::updateLayerList()
{
    m_layerTree->clear();
    for (int i = 0; i < m_layers.size(); ++i) {
        const GerberLayer &layer = m_layers[i];
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, layer.fileName);
        item->setText(1, QString::number(layer.polygons.size()));
        item->setText(2, QString::number(layer.segments.size()));
        if (!layer.boundingRect.isEmpty())
            item->setText(3, QString("%1 x %2 mm")
                .arg(layer.boundingRect.width(), 0, 'f', 1)
                .arg(layer.boundingRect.height(), 0, 'f', 1));
        if (!layer.valid)
            item->setForeground(0, QColor(255, 80, 80));
        m_layerTree->addTopLevelItem(item);
    }
}
void MainWindow::onGenerateGCode() {
    if (currentLayerIndex() < 0 || currentLayerIndex() >= m_layers.size()) {
        QMessageBox::warning(this, "G-Code", "No layer selected");
        return;
    }

    const GerberLayer& layer = m_layers[currentLayerIndex()];

    // Генерируем точки дозирования
    QVector<DispensePoint> points = m_gcodeGen->generateDispensePoints(layer);

    if (points.isEmpty()) {
        QMessageBox::information(this, "G-Code", "No dispense points found");
        return;
    }

    // Показываем окно
    if (!m_gcodeWindow) {
        m_gcodeWindow = new GCodeWindow(this);
    }

    m_gcodeWindow->setGCode(points, m_gcodeGen->getBoundingRect(), layer.fileName);
    m_gcodeWindow->show();
    m_gcodeWindow->raise();
}
