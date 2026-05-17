#include "gerbertexteditor.h"
#include <QTextBlock>
#include <QScrollBar>

GerberTextEditor::GerberTextEditor(QWidget* parent)
    : QPlainTextEdit(parent) {
    setPlaceholderText("Gerber source (RS-274X)...");
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        if (!m_silent)
            emit editFinished();
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &GerberTextEditor::onCursorMoved);
}

void GerberTextEditor::setPlainTextSilent(const QString& text) {
    m_silent = true;
    setPlainText(text);
    clearObjectHighlight();
    m_silent = false;
}

void GerberTextEditor::applyHighlightRanges(const QVector<GerberHighlightRange>& ranges) {
    QList<QTextEdit::ExtraSelection> extras;

    for (const GerberHighlightRange& range : ranges) {
        if (range.start < 0 || range.end <= range.start)
            continue;

        QTextEdit::ExtraSelection sel;
        sel.cursor = textCursor();
        const int docEnd = document()->characterCount();
        const int selEnd = qBound(range.start + 1, range.end, docEnd);
        sel.cursor.setPosition(range.start);
        sel.cursor.setPosition(selEnd, QTextCursor::KeepAnchor);

        if (range.primary) {
            sel.format.setBackground(QColor(255, 200, 60));
            sel.format.setForeground(QColor(20, 20, 20));
        } else {
            sel.format.setBackground(QColor(70, 110, 90));
            sel.format.setForeground(QColor(220, 240, 220));
        }
        sel.format.setProperty(QTextFormat::FullWidthSelection, false);
        extras.append(sel);
    }

    setExtraSelections(extras);
}

void GerberTextEditor::setSourceContext(const GerberSourceMap* map, const GerberLayer* layer) {
    m_sourceMap = map;
    m_layer = layer;
}

void GerberTextEditor::highlightVertex(int polyIndex, int pointIndex) {
    if (!m_sourceMap || polyIndex < 0) {
        clearObjectHighlight();
        return;
    }

    const QVector<GerberHighlightRange> ranges = m_sourceMap->rangesForVertex(polyIndex, pointIndex);
    applyHighlightRanges(ranges);
    for (const GerberHighlightRange& range : ranges) {
        if (!range.primary)
            continue;
        QTextCursor c = textCursor();
        c.setPosition(range.start);
        setTextCursor(c);
        centerCursor();
        break;
    }
}

void GerberTextEditor::highlightPolygon(int polyIndex) {
    if (!m_sourceMap || polyIndex < 0) {
        clearObjectHighlight();
        return;
    }

    const QVector<GerberHighlightRange> ranges = m_sourceMap->rangesForPolygon(polyIndex);
    applyHighlightRanges(ranges);

    for (const GerberHighlightRange& range : ranges) {
        QTextCursor c = textCursor();
        c.setPosition(range.start);
        setTextCursor(c);
        centerCursor();
        break;
    }
}

void GerberTextEditor::clearObjectHighlight() {
    setExtraSelections({});
}

void GerberTextEditor::onCursorMoved() {
    if (m_silent || !m_sourceMap || !m_layer)
        return;

    int pi = -1, vi = -1;
    if (m_sourceMap->vertexAtCursor(textCursor().position(), *m_layer, &pi, &vi))
        emit objectAtCursor(pi, vi);
}
