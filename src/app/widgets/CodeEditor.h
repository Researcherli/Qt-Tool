#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QColor>
#include <QPlainTextEdit>

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
QT_END_NAMESPACE

namespace est {

class LineNumberArea;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const;

    void setLineNumberAreaBackground(const QColor &color);
    void setLineNumberAreaTextColor(const QColor &color);

    void setEditorFont(const QFont &font);
    QFont editorFont() const;
    void zoomIn();
    void zoomOut();
    void zoomReset();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    bool m_showLineNumbers = true;
    QColor m_lineNumberBgColor{238, 245, 251};     // #EEF5FB 匹配浅色主题
    QColor m_lineNumberTextColor{138, 155, 181};   // #8A9BB5
};

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
    {}

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

} // namespace est

#endif // CODEEDITOR_H
