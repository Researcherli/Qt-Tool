#ifndef EST_HEXVIEWERWIDGET_H
#define EST_HEXVIEWERWIDGET_H

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
        void highlightMatches(const QVector<qsizetype> &matches, qsizetype currentOffset);

    signals:
        void offsetChanged(qint64 offset, uchar byteValue, const QString &asciiText);

    private:
        void render();
        void selectOffset(qint64 offset);

        QPlainTextEdit *m_textEdit = nullptr;
        QByteArray m_data;
        QVector<qsizetype> m_matches;
        qsizetype m_currentOffset = -1;
    };

} // namespace est

#endif // EST_HEXVIEWERWIDGET_H
