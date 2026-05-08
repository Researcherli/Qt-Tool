#include "widgets/HexViewerWidget.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPaintEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>

#include <functional>

namespace est
{

    namespace
    {

        QString asciiForByte(uchar byte)
        {
            return (byte >= 0x20 && byte <= 0x7E)
                       ? QString(QChar(byte))
                       : QStringLiteral(".");
        }

        int hexColumnForOffset(qint64 offset)
        {
            return 10 + static_cast<int>(offset % 16) * 3;
        }

        int asciiColumnForOffset(qint64 offset)
        {
            return 59 + static_cast<int>(offset % 16);
        }

        class FileOverviewWidget : public QWidget
        {
        public:
            explicit FileOverviewWidget(QWidget *parent = nullptr)
                : QWidget(parent)
            {
                setObjectName(QStringLiteral("hexFileOverview"));
                setMinimumWidth(22);
                setMaximumWidth(26);
                setCursor(Qt::PointingHandCursor);
            }

            void setData(const QByteArray &data)
            {
                m_data = data;
                update();
            }

            void setOffsetRequestedCallback(std::function<void(qint64)> callback)
            {
                m_offsetRequested = std::move(callback);
            }

            void setHighlights(const QVector<qsizetype> &matches,
                               int matchLength,
                               qint64 rangeOffset,
                               int rangeLength,
                               qsizetype currentOffset)
            {
                m_matches = matches;
                m_matchLength = qMax(1, matchLength);
                m_rangeOffset = rangeOffset;
                m_rangeLength = rangeLength;
                m_currentOffset = currentOffset;
                update();
            }

        protected:
            void paintEvent(QPaintEvent *) override
            {
                QPainter painter(this);
                painter.fillRect(rect(), QColor(QStringLiteral("#EAF2FA")));
                painter.setPen(QColor(QStringLiteral("#C9DAE8")));
                painter.drawRect(rect().adjusted(0, 0, -1, -1));

                if (m_data.isEmpty() || height() <= 2)
                {
                    return;
                }

                const int contentTop = 2;
                const int contentHeight = qMax(1, height() - 4);
                for (int y = 0; y < contentHeight; ++y)
                {
                    const qsizetype index = qMin<qsizetype>(
                        m_data.size() - 1,
                        static_cast<qsizetype>((static_cast<double>(y) / contentHeight) * m_data.size()));
                    const int value = static_cast<uchar>(m_data.at(index));
                    const int shade = 245 - value / 5;
                    painter.setPen(QColor(shade, shade, shade));
                    painter.drawLine(3, contentTop + y, width() - 4, contentTop + y);
                }

                for (qsizetype matchOffset : m_matches)
                {
                    drawMarker(&painter, matchOffset, m_matchLength, QColor(QStringLiteral("#D94600")));
                }

                if (m_rangeOffset >= 0 && m_rangeLength > 0)
                {
                    drawMarker(&painter, m_rangeOffset, m_rangeLength, QColor(QStringLiteral("#FFB020")));
                }
                if (m_currentOffset >= 0)
                {
                    drawMarker(&painter, m_currentOffset, 1, QColor(QStringLiteral("#005BBB")));
                }
            }

            void mousePressEvent(QMouseEvent *event) override
            {
                requestOffset(event->position().toPoint().y());
            }

            void mouseMoveEvent(QMouseEvent *event) override
            {
                if ((event->buttons() & Qt::LeftButton) != 0)
                {
                    requestOffset(event->position().toPoint().y());
                }
            }

        private:
            void drawMarker(QPainter *painter, qint64 offset, int length, const QColor &color)
            {
                if (m_data.isEmpty() || offset < 0 || offset >= m_data.size())
                {
                    return;
                }

                const int contentHeight = qMax(1, height() - 4);
                const int y = 2 + static_cast<int>((static_cast<double>(offset) / m_data.size()) * contentHeight);
                const int markerHeight = qMax(2, static_cast<int>((static_cast<double>(length) / m_data.size()) * contentHeight));
                const QRect markerRect(1, y, width() - 2, qMax(4, markerHeight));
                painter->fillRect(markerRect, color);
                painter->setPen(QColor(QStringLiteral("#0B2540")));
                painter->drawRect(markerRect.adjusted(0, 0, -1, -1));
            }

            void requestOffset(int y)
            {
                if (m_data.isEmpty() || m_offsetRequested == nullptr)
                {
                    return;
                }

                const int contentHeight = qMax(1, height() - 4);
                const int clampedY = qBound(2, y, height() - 2) - 2;
                const double ratio = qBound(0.0, static_cast<double>(clampedY) / contentHeight, 1.0);
                const qint64 offset = qMin<qint64>(
                    m_data.size() - 1,
                    static_cast<qint64>(ratio * m_data.size()));
                m_offsetRequested(offset);
            }

            QByteArray m_data;
            QVector<qsizetype> m_matches;
            int m_matchLength = 1;
            qint64 m_rangeOffset = -1;
            int m_rangeLength = 0;
            qsizetype m_currentOffset = -1;
            std::function<void(qint64)> m_offsetRequested;
        };

    } // namespace

    HexViewerWidget::HexViewerWidget(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        m_textEdit = new QPlainTextEdit(this);
        m_textEdit->setReadOnly(true);
        m_textEdit->setPlaceholderText(tr("请先导入 BIN 文件"));
        auto *overviewWidget = new FileOverviewWidget(this);
        overviewWidget->setOffsetRequestedCallback([this](qint64 offset) {
            jumpToOffset(offset);
        });
        m_overviewWidget = overviewWidget;
        layout->addWidget(m_textEdit, 1);
        layout->addWidget(m_overviewWidget);
    }

    void HexViewerWidget::setData(const QByteArray &data)
    {
        m_data = data;
        m_currentOffset = data.isEmpty() ? -1 : 0;
        render();
        refreshOverview();
        if (!data.isEmpty())
        {
            selectOffset(0);
        }
    }

    void HexViewerWidget::clear()
    {
        m_data.clear();
        m_matches.clear();
        m_matchLength = 1;
        m_currentOffset = -1;
        m_highlightRangeOffset = -1;
        m_highlightRangeLength = 0;
        m_textEdit->clear();
        refreshOverview();
    }

    void HexViewerWidget::jumpToOffset(qint64 offset)
    {
        if (offset < 0 || offset >= m_data.size())
        {
            return;
        }
        selectOffset(offset);
    }

    void HexViewerWidget::highlightRange(qint64 offset, int length)
    {
        if (offset < 0 || offset >= m_data.size() || length <= 0)
        {
            return;
        }

        m_highlightRangeOffset = offset;
        m_highlightRangeLength = qMin<int>(length, static_cast<int>(m_data.size() - offset));
        selectOffset(offset);
        refreshOverview();
    }

    void HexViewerWidget::highlightMatches(const QVector<qsizetype> &matches,
                                           qsizetype currentOffset,
                                           int matchLength)
    {
        m_matches = matches;
        m_matchLength = qMax(1, matchLength);
        m_currentOffset = currentOffset;
        if (currentOffset >= 0)
        {
            selectOffset(currentOffset);
        }
        refreshOverview();
    }

    void HexViewerWidget::render()
    {
        if (m_data.isEmpty())
        {
            m_textEdit->clear();
            return;
        }

        QString content;
        content.reserve(m_data.size() * 4);
        for (int offset = 0; offset < m_data.size(); offset += 16)
        {
            content.append(QStringLiteral("%1  ").arg(offset, 8, 16, QLatin1Char('0')).toUpper());

            QString hexPart;
            QString asciiPart;
            for (int index = 0; index < 16; ++index)
            {
                const int byteIndex = offset + index;
                if (byteIndex < m_data.size())
                {
                    const uchar byte = static_cast<uchar>(m_data.at(byteIndex));
                    hexPart.append(QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0')).toUpper());
                    asciiPart.append(asciiForByte(byte));
                }
                else
                {
                    hexPart.append(QStringLiteral("   "));
                    asciiPart.append(QStringLiteral(" "));
                }
            }

            content.append(hexPart);
            content.append(QStringLiteral(" "));
            content.append(asciiPart);
            content.append(QChar('\n'));
        }

        m_textEdit->setPlainText(content);
    }

    void HexViewerWidget::selectOffset(qint64 offset)
    {
        if (offset < 0 || offset >= m_data.size())
        {
            return;
        }

        const int blockNumber = static_cast<int>(offset / 16);
        QTextBlock block = m_textEdit->document()->findBlockByNumber(blockNumber);
        if (block.isValid())
        {
            QTextCursor visibleCursor(block);
            visibleCursor.setPosition(block.position() + hexColumnForOffset(offset));
            m_textEdit->setTextCursor(visibleCursor);
            m_textEdit->centerCursor();

            QTextCursor highlightCursor(block);
            highlightCursor.setPosition(block.position() + hexColumnForOffset(offset));
            highlightCursor.setPosition(block.position() + hexColumnForOffset(offset) + 2,
                                        QTextCursor::KeepAnchor);

            QList<QTextEdit::ExtraSelection> selections;
            selections.append(rangeSelections());
            selections.append(matchSelections());

            QTextEdit::ExtraSelection currentSelection;
            currentSelection.cursor = highlightCursor;
            const bool currentInsideRange = m_highlightRangeOffset >= 0
                                            && offset >= m_highlightRangeOffset
                                            && offset < m_highlightRangeOffset + m_highlightRangeLength;
            currentSelection.format.setBackground(currentInsideRange
                                                      ? QColor(QStringLiteral("#FFB020"))
                                                      : QColor(QStringLiteral("#2F7FB5")));
            currentSelection.format.setForeground(currentInsideRange
                                                      ? QColor(QStringLiteral("#0B2540"))
                                                      : QColor(QStringLiteral("#FFFFFF")));
            selections.append(currentSelection);
            m_textEdit->setExtraSelections(selections);
        }

        m_currentOffset = offset;
        refreshOverview();
        const uchar byte = static_cast<uchar>(m_data.at(offset));
        emit offsetChanged(offset, byte, asciiForByte(byte));
    }

    void HexViewerWidget::refreshOverview()
    {
        auto *overview = static_cast<FileOverviewWidget *>(m_overviewWidget);
        if (overview == nullptr)
        {
            return;
        }

        overview->setData(m_data);
        overview->setHighlights(m_matches,
                                m_matchLength,
                                m_highlightRangeOffset,
                                m_highlightRangeLength,
                                m_currentOffset);
    }

    QList<QTextEdit::ExtraSelection> HexViewerWidget::rangeSelections() const
    {
        return selectionsForRange(m_highlightRangeOffset,
                                  m_highlightRangeLength,
                                  QColor(QStringLiteral("#FFB020")),
                                  QColor(QStringLiteral("#0B2540")));
    }

    QList<QTextEdit::ExtraSelection> HexViewerWidget::matchSelections() const
    {
        QList<QTextEdit::ExtraSelection> selections;
        for (qsizetype matchOffset : m_matches)
        {
            selections.append(selectionsForRange(matchOffset,
                                                m_matchLength,
                                                QColor(QStringLiteral("#FFF3B0")),
                                                QColor(QStringLiteral("#173E63"))));
        }
        return selections;
    }

    QList<QTextEdit::ExtraSelection> HexViewerWidget::selectionsForRange(qint64 rangeOffset,
                                                                         int rangeLength,
                                                                         const QColor &background,
                                                                         const QColor &foreground) const
    {
        QList<QTextEdit::ExtraSelection> selections;
        if (rangeOffset < 0 || rangeLength <= 0 || m_data.isEmpty())
        {
            return selections;
        }

        const qint64 rangeEnd = qMin<qint64>(rangeOffset + rangeLength, m_data.size());
        for (qint64 offset = rangeOffset; offset < rangeEnd; ++offset)
        {
            QTextBlock block = m_textEdit->document()->findBlockByNumber(static_cast<int>(offset / 16));
            if (!block.isValid())
            {
                continue;
            }

            QTextCursor hexCursor(block);
            hexCursor.setPosition(block.position() + hexColumnForOffset(offset));
            hexCursor.setPosition(block.position() + hexColumnForOffset(offset) + 2,
                                  QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection hexSelection;
            hexSelection.cursor = hexCursor;
            hexSelection.format.setBackground(background);
            hexSelection.format.setForeground(foreground);
            selections.append(hexSelection);
        }

        qint64 lineStart = rangeOffset;
        while (lineStart < rangeEnd)
        {
            const qint64 nextLine = ((lineStart / 16) + 1) * 16;
            const qint64 lineEnd = qMin(rangeEnd, nextLine);
            QTextBlock block = m_textEdit->document()->findBlockByNumber(static_cast<int>(lineStart / 16));
            if (block.isValid())
            {
                QTextCursor asciiCursor(block);
                asciiCursor.setPosition(block.position() + asciiColumnForOffset(lineStart));
                asciiCursor.setPosition(block.position() + asciiColumnForOffset(lineStart)
                                            + static_cast<int>(lineEnd - lineStart),
                                        QTextCursor::KeepAnchor);

                QTextEdit::ExtraSelection asciiSelection;
                asciiSelection.cursor = asciiCursor;
                asciiSelection.format.setBackground(background);
                asciiSelection.format.setForeground(foreground);
                selections.append(asciiSelection);
            }
            lineStart = lineEnd;
        }

        return selections;
    }

} // namespace est
