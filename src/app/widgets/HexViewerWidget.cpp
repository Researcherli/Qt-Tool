#include "widgets/HexViewerWidget.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

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

    } // namespace

    HexViewerWidget::HexViewerWidget(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_textEdit = new QPlainTextEdit(this);
        m_textEdit->setReadOnly(true);
        m_textEdit->setPlaceholderText(tr("请先导入 BIN 文件"));
        layout->addWidget(m_textEdit);
    }

    void HexViewerWidget::setData(const QByteArray &data)
    {
        m_data = data;
        m_currentOffset = data.isEmpty() ? -1 : 0;
        render();
        if (!data.isEmpty())
        {
            selectOffset(0);
        }
    }

    void HexViewerWidget::clear()
    {
        m_data.clear();
        m_matches.clear();
        m_currentOffset = -1;
        m_textEdit->clear();
    }

    void HexViewerWidget::jumpToOffset(qint64 offset)
    {
        if (offset < 0 || offset >= m_data.size())
        {
            return;
        }
        selectOffset(offset);
    }

    void HexViewerWidget::highlightMatches(const QVector<qsizetype> &matches, qsizetype currentOffset)
    {
        m_matches = matches;
        m_currentOffset = currentOffset;
        if (currentOffset >= 0)
        {
            selectOffset(currentOffset);
        }
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
            QTextCursor cursor(block);
            m_textEdit->setTextCursor(cursor);
            m_textEdit->centerCursor();

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(QColor(QStringLiteral("#DCECF8")));
            selection.format.setForeground(QColor(QStringLiteral("#173E63")));
            m_textEdit->setExtraSelections({selection});
        }

        m_currentOffset = offset;
        const uchar byte = static_cast<uchar>(m_data.at(offset));
        emit offsetChanged(offset, byte, asciiForByte(byte));
    }

} // namespace est
