#include "CodeEditor.h"

#include <QPainter>
#include <QTextBlock>

namespace est {

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);
    lineNumberArea->setAutoFillBackground(true);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth()
{
    if (!m_showLineNumbers)
        return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    if (!m_showLineNumbers) {
        setViewportMargins(0, 0, 0, 0);
        return;
    }
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    if (!m_showLineNumbers) {
        lineNumberArea->hide();
        return;
    }

    lineNumberArea->show();
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine()
{
    if (isReadOnly()) {
        return;
    }

    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), m_lineNumberBgColor);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int lineHeight = fontMetrics().lineSpacing();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + lineHeight;

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(m_lineNumberTextColor);
            painter.drawText(0, top, lineNumberArea->width(), lineHeight,
                             Qt::AlignRight | Qt::AlignVCenter, number);
        }

        block = block.next();
        top += lineHeight;
        bottom = top + lineHeight;
        ++blockNumber;
    }
}

void CodeEditor::setLineNumbersVisible(bool visible)
{
    if (m_showLineNumbers == visible)
        return;

    m_showLineNumbers = visible;
    updateLineNumberAreaWidth(0);

    if (visible) {
        lineNumberArea->show();
    } else {
        lineNumberArea->hide();
    }

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

bool CodeEditor::lineNumbersVisible() const
{
    return m_showLineNumbers;
}

void CodeEditor::setLineNumberAreaBackground(const QColor &color)
{
    m_lineNumberBgColor = color;
    lineNumberArea->update();
}

void CodeEditor::setLineNumberAreaTextColor(const QColor &color)
{
    m_lineNumberTextColor = color;
    lineNumberArea->update();
}

    void CodeEditor::setEditorFont(const QFont &font)
    {
        setFont(font);
        updateLineNumberAreaWidth(0);
    }

    QFont CodeEditor::editorFont() const
    {
        return font();
    }

    void CodeEditor::zoomIn()
    {
        QFont f = font();
        f.setPointSize(std::min(48, f.pointSize() + 1));
        setEditorFont(f);
    }

    void CodeEditor::zoomOut()
    {
        QFont f = font();
        f.setPointSize(std::max(6, f.pointSize() - 1));
        setEditorFont(f);
    }

    void CodeEditor::zoomReset()
    {
        QFont f = font();
        f.setPointSize(10);
        setEditorFont(f);
    }

} // namespace est
