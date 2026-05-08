#ifndef EST_HEXVIEWERWIDGET_H
#define EST_HEXVIEWERWIDGET_H

#include <QTextEdit>
#include <QWidget>

class QPlainTextEdit;

namespace est
{

    class HexViewerWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit HexViewerWidget(QWidget *parent = nullptr);

        void setData(const QByteArray &data);
        void clear();
        void jumpToOffset(qint64 offset);
        void highlightRange(qint64 offset, int length);
        void highlightMatches(const QVector<qsizetype> &matches, qsizetype currentOffset, int matchLength = 1);

    signals:
        void offsetChanged(qint64 offset, uchar byteValue, const QString &asciiText);

    private:
        void render();
        void selectOffset(qint64 offset);
        void refreshOverview();
        QList<QTextEdit::ExtraSelection> rangeSelections() const;
        QList<QTextEdit::ExtraSelection> matchSelections() const;
        QList<QTextEdit::ExtraSelection> selectionsForRange(qint64 rangeOffset,
                                                            int rangeLength,
                                                            const QColor &background,
                                                            const QColor &foreground) const;

        QPlainTextEdit *m_textEdit = nullptr;
        QWidget *m_overviewWidget = nullptr;
        QByteArray m_data;
        QVector<qsizetype> m_matches;
        int m_matchLength = 1;
        qsizetype m_currentOffset = -1;
        qint64 m_highlightRangeOffset = -1;
        int m_highlightRangeLength = 0;
    };

} // namespace est

#endif // EST_HEXVIEWERWIDGET_H
