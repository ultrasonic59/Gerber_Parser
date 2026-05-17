#ifndef GERBERTEXTEDITOR_H
#define GERBERTEXTEDITOR_H

#include <QPlainTextEdit>
#include "gerbersourcemap.h"

class GerberTextEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit GerberTextEditor(QWidget* parent = nullptr);

    void setPlainTextSilent(const QString& text);
    bool isSilentUpdate() const { return m_silent; }

    void setSourceContext(const GerberSourceMap* map, const GerberLayer* layer);
    void highlightVertex(int polyIndex, int pointIndex);
    void highlightPolygon(int polyIndex);
    void clearObjectHighlight();

signals:
    void editFinished();
    void objectAtCursor(int polyIndex, int pointIndex);

private slots:
    void onCursorMoved();

private:
    void applyHighlightRanges(const QVector<GerberHighlightRange>& ranges);

    bool m_silent = false;
    const GerberSourceMap* m_sourceMap = nullptr;
    const GerberLayer* m_layer = nullptr;
};

#endif // GERBERTEXTEDITOR_H
