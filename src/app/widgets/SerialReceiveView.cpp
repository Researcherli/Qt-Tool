#include "widgets/SerialReceiveView.h"

#include "databus/DataPacket.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextLayout>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace est
{

    namespace
    {
        const QColor kRxBadgeColor(QStringLiteral("#006B3F"));
        const QColor kTxBadgeColor(QStringLiteral("#B91C1C"));
        const QColor kPortBadgeColor(QStringLiteral("#5B21B6"));

        QString displayPayload(const QByteArray &payload, const QString &mode)
        {
            if (mode == QStringLiteral("hex"))
            {
                return QString::fromLatin1(payload.toHex(' ')).toUpper();
            }

            QString text = QString::fromUtf8(payload);
            text.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
            text.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
            if (text.isEmpty() && !payload.isEmpty())
            {
                text = QObject::tr("<二进制数据 %1 字节>").arg(payload.size());
            }
            return text;
        }

        class ZoomablePlainTextEdit : public QPlainTextEdit
        {
        public:
            explicit ZoomablePlainTextEdit(QWidget *parent = nullptr)
                : QPlainTextEdit(parent)
            {
            }

        protected:
            void wheelEvent(QWheelEvent *event) override
            {
                if ((event->modifiers() & Qt::ControlModifier) == 0)
                {
                    QPlainTextEdit::wheelEvent(event);
                    return;
                }

                const int delta = event->angleDelta().y();
                if (delta == 0)
                {
                    event->accept();
                    return;
                }

                QFont currentFont = font();
                qreal pointSize = currentFont.pointSizeF();
                if (pointSize <= 0)
                {
                    pointSize = 10.0;
                }

                pointSize += delta > 0 ? 1.0 : -1.0;
                pointSize = std::clamp(pointSize, 8.0, 32.0);
                currentFont.setPointSizeF(pointSize);
                setFont(currentFont);
                event->accept();
            }

            void paintEvent(QPaintEvent *event) override
            {
                QPlainTextEdit::paintEvent(event);

                if (!property("directionBadgeEnabled").toBool())
                {
                    return;
                }

                QPainter painter(viewport());
                painter.setRenderHint(QPainter::Antialiasing, true);

                QTextBlock block = firstVisibleBlock();
                const QRegularExpression fixedFieldPattern(QStringLiteral("(?:\\]\\s+|^)(RX|TX)(?:\\s+([^:\\s]+))?"));
                while (block.isValid())
                {
                    const QRectF blockRect = blockBoundingGeometry(block).translated(contentOffset());
                    if (blockRect.top() > viewport()->height())
                    {
                        break;
                    }
                    if (blockRect.bottom() >= 0)
                    {
                        const QString text = block.text();
                        const QRegularExpressionMatch match = fixedFieldPattern.match(text);
                        if (match.hasMatch())
                        {
                            QTextLayout *layout = block.layout();
                            for (int i = 0; i < layout->lineCount(); ++i)
                            {
                                const QTextLine line = layout->lineAt(i);
                                const auto drawBadge = [&](int captureIndex, const QColor &badgeColor) {
                                    if (match.capturedLength(captureIndex) <= 0)
                                    {
                                        return;
                                    }

                                    const int tokenStart = match.capturedStart(captureIndex);
                                    const int tokenEnd = tokenStart + match.capturedLength(captureIndex);
                                    if (tokenStart < line.textStart() || tokenStart >= line.textStart() + line.textLength())
                                    {
                                        return;
                                    }

                                    qreal startX = line.cursorToX(tokenStart);
                                    qreal endX = line.cursorToX(tokenEnd);
                                    if (endX < startX)
                                    {
                                        std::swap(startX, endX);
                                    }

                                    const QRectF textRect(blockRect.left() + startX,
                                                          blockRect.top() + line.y(),
                                                          endX - startX,
                                                          line.height());
                                    const QRectF badgeRect = textRect.adjusted(-4.0, 4.0, 4.0, -4.0);

                                    painter.setPen(Qt::NoPen);
                                    painter.setBrush(badgeColor);
                                    painter.drawRoundedRect(badgeRect, 2.0, 2.0);
                                    painter.setPen(Qt::white);
                                    painter.setFont(font());
                                    painter.drawText(textRect, Qt::AlignCenter, match.captured(captureIndex));
                                };

                                drawBadge(1, match.captured(1) == QStringLiteral("TX") ? kTxBadgeColor : kRxBadgeColor);
                                drawBadge(2, kPortBadgeColor);
                            }
                        }
                    }
                    block = block.next();
                }
            }
        };

        class ReceiveFixedFieldHighlighter : public QSyntaxHighlighter
        {
        public:
            explicit ReceiveFixedFieldHighlighter(QTextDocument *document)
                : QSyntaxHighlighter(document)
            {
                setProperty("highlightEnabled", true);

                m_timestampFormat.setForeground(QColor(QStringLiteral("#64748B")));

                m_rxFormat.setForeground(kRxBadgeColor);
                m_rxFormat.setFontWeight(QFont::Bold);

                m_txFormat.setForeground(kTxBadgeColor);
                m_txFormat.setFontWeight(QFont::Bold);

                m_portFormat.setForeground(kPortBadgeColor);
                m_portFormat.setFontWeight(QFont::DemiBold);
            }

        protected:
            void highlightBlock(const QString &text) override
            {
                if (!property("highlightEnabled").toBool())
                {
                    return;
                }

                const QRegularExpressionMatch timestampMatch = m_timestampPattern.match(text);
                if (timestampMatch.hasMatch())
                {
                    setFormat(timestampMatch.capturedStart(), timestampMatch.capturedLength(), m_timestampFormat);
                }

                const QRegularExpressionMatch directionMatch = m_directionPattern.match(text);
                if (!directionMatch.hasMatch())
                {
                    return;
                }

                const QString direction = directionMatch.captured(1);
                const QTextCharFormat &format = direction == QStringLiteral("TX") ? m_txFormat : m_rxFormat;
                setFormat(directionMatch.capturedStart(1), directionMatch.capturedLength(1), format);
                if (directionMatch.capturedLength(2) > 0)
                {
                    setFormat(directionMatch.capturedStart(2), directionMatch.capturedLength(2), m_portFormat);
                }
            }

        private:
            QRegularExpression m_timestampPattern = QRegularExpression(QStringLiteral("^\\[[^\\]]+\\]"));
            QRegularExpression m_directionPattern = QRegularExpression(QStringLiteral("(?:\\]\\s+|^)(RX|TX)(?:\\s+([^:\\s]+))?"));
            QTextCharFormat m_timestampFormat;
            QTextCharFormat m_rxFormat;
            QTextCharFormat m_txFormat;
            QTextCharFormat m_portFormat;
        };

    } // namespace

    SerialReceiveView::SerialReceiveView(QWidget *parent)
        : QWidget(parent)
    {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(6);
        setObjectName(QStringLiteral("serialReceiveView"));

        auto *toolLayout = new QHBoxLayout();
        toolLayout->setContentsMargins(0, 0, 0, 0);
        toolLayout->setSpacing(8);

        auto *titleLabel = new QLabel(tr("数据区"), this);
        titleLabel->setObjectName(QStringLiteral("receiveSectionTitle"));
        toolLayout->addWidget(titleLabel);

        m_modeCombo = new QComboBox(this);
        m_modeCombo->addItem(tr("文本"), QStringLiteral("text"));
        m_modeCombo->addItem(QStringLiteral("HEX"), QStringLiteral("hex"));
        m_modeCombo->setMinimumWidth(72);
        m_modeCombo->setMaximumWidth(92);
        m_autoScrollCheckBox = new QCheckBox(tr("自动滚动"), this);
        m_autoScrollCheckBox->setChecked(true);
        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setPlaceholderText(tr("搜索接收内容"));
        m_searchEdit->setMinimumWidth(180);
        auto *prevButton = new QPushButton(tr("上一个"), this);
        auto *nextButton = new QPushButton(tr("下一个"), this);
        auto *clearButton = new QPushButton(tr("清空"), this);
        clearButton->setObjectName(QStringLiteral("receiveClearDataButton"));
        auto *saveButton = new QPushButton(tr("保存日志"), this);
        auto *settingsButton = new QPushButton(tr("设置"), this);

        toolLayout->addWidget(m_modeCombo);
        toolLayout->addWidget(m_autoScrollCheckBox);
        toolLayout->addWidget(m_searchEdit, 1);
        toolLayout->addWidget(prevButton);
        toolLayout->addWidget(nextButton);
        toolLayout->addWidget(clearButton);
        toolLayout->addWidget(saveButton);
        toolLayout->addWidget(settingsButton);

        m_textEdit = new ZoomablePlainTextEdit(this);
        m_textEdit->setObjectName(QStringLiteral("serialReceiveTextEdit"));
        // Increase receive area font size to 20px to match send input and quick commands
        QFont recvFont = m_textEdit->font();
        recvFont.setPixelSize(20);
        m_textEdit->setFont(recvFont);
        if (m_textEdit->document() != nullptr) {
            m_textEdit->document()->setDefaultFont(recvFont);
        }
        m_textEdit->setStyleSheet(QStringLiteral("font-size:20px;"));
        m_textEdit->setReadOnly(true);
        m_textEdit->setPlaceholderText(tr("暂无接收数据，打开串口后开始监听"));
        // 限制文本框行数，使用 Qt 内置的 setMaximumBlockCount 自动淘汰旧行
        // 避免手动 deleteChar/removeSelectedText 导致的 QTextDocument 内部结构损坏
        m_textEdit->setMaximumBlockCount(5000);
        m_fixedFieldHighlighter = new ReceiveFixedFieldHighlighter(m_textEdit->document());
        applyHighlightSettings();

        rootLayout->addLayout(toolLayout);
        rootLayout->addWidget(m_textEdit, 1);

        connect(m_modeCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { renderAll(); });
        connect(prevButton, &QPushButton::clicked, this, [this]() { findNext(false); });
        connect(nextButton, &QPushButton::clicked, this, [this]() { findNext(true); });
        connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() { findNext(true); });
        connect(clearButton, &QPushButton::clicked, m_searchEdit, &QLineEdit::clear);
        connect(settingsButton, &QPushButton::clicked, this, [this]() { showDisplaySettingsDialog(); });
        connect(saveButton, &QPushButton::clicked, this, [this]() {
            const QString filePath = QFileDialog::getSaveFileName(this, tr("保存串口日志"), QStringLiteral("serial_log.txt"), tr("文本日志 (*.txt *.log)"));
            if (filePath.isEmpty())
            {
                return;
            }
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                emit statusMessageGenerated(tr("串口日志保存失败：%1").arg(filePath));
                return;
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << m_textEdit->toPlainText();
            emit statusMessageGenerated(tr("串口日志已保存到：%1").arg(filePath));
        });
    }

    void SerialReceiveView::appendPacket(const DataPacket &packet)
    {
        LogEntry entry;
        entry.timestamp = packet.timestamp;
        entry.payload = packet.rawPayload;
        entry.direction = packet.metadata.value(QStringLiteral("direction"), QStringLiteral("rx")).toString().toUpper();
        entry.portName = packet.metadata.value(QStringLiteral("portName")).toString();
        m_entries.append(entry);

        // 限制内存中的记录数量，防止长时间运行导致 OOM
        const int kMaxEntries = 5000;
        if (m_entries.size() > kMaxEntries)
        {
            m_entries.removeFirst();
        }

        // 文本框行数限制已由 setMaximumBlockCount(5000) 自动管理，无需手动删除

        const bool autoScroll = m_autoScrollCheckBox->isChecked();
        const int previousScrollPosition = m_textEdit->verticalScrollBar()->value();
        m_textEdit->appendPlainText(formatEntry(entry));

        if (autoScroll)
        {
            m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
        }
        else
        {
            m_textEdit->verticalScrollBar()->setValue(previousScrollPosition);
        }
    }

    void SerialReceiveView::clearEntries()
    {
        m_entries.clear();
        m_textEdit->clear();
        emit entriesCleared();
        emit statusMessageGenerated(tr("已清空接收区"));
    }

    bool SerialReceiveView::showDisplaySettingsDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("接收区显示设置"));

        auto *layout = new QVBoxLayout(&dialog);
        auto *formLayout = new QFormLayout();

        auto *highlightCheckBox = new QCheckBox(tr("高亮固定字段"), &dialog);
        highlightCheckBox->setObjectName(QStringLiteral("receiveHighlightCheckBox"));
        highlightCheckBox->setChecked(m_fixedFieldHighlightEnabled);
        formLayout->addRow(tr("显示高亮"), highlightCheckBox);
        layout->addLayout(formLayout);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted)
        {
            return false;
        }

        m_fixedFieldHighlightEnabled = highlightCheckBox->isChecked();
        applyHighlightSettings();
        emit statusMessageGenerated(m_fixedFieldHighlightEnabled
                                        ? tr("接收区固定字段高亮已开启")
                                        : tr("接收区固定字段高亮已关闭"));
        return true;
    }

    void SerialReceiveView::setTimestampVisible(bool visible)
    {
        if (m_timestampVisible == visible)
        {
            return;
        }

        m_timestampVisible = visible;
        renderAll();
    }

    bool SerialReceiveView::timestampVisible() const
    {
        return m_timestampVisible;
    }

    QString SerialReceiveView::formatEntry(const LogEntry &entry) const
    {
        const QString timestampText = QDateTime::fromMSecsSinceEpoch(entry.timestamp)
                                          .toString(QStringLiteral("HH:mm:ss.zzz"));
        const QString prefix = entry.portName.isEmpty() ? entry.direction : tr("%1 %2").arg(entry.direction, entry.portName);
        if (!m_timestampVisible)
        {
            return QStringLiteral("%1: %2")
                .arg(prefix, displayPayload(entry.payload, m_modeCombo->currentData().toString()));
        }

        return QStringLiteral("[%1] %2: %3")
            .arg(timestampText, prefix, displayPayload(entry.payload, m_modeCombo->currentData().toString()));
    }

    void SerialReceiveView::renderAll()
    {
        m_textEdit->clear();
        for (const LogEntry &entry : std::as_const(m_entries))
        {
            m_textEdit->appendPlainText(formatEntry(entry));
        }
        if (m_autoScrollCheckBox->isChecked())
        {
            m_textEdit->verticalScrollBar()->setValue(m_textEdit->verticalScrollBar()->maximum());
        }
    }

    void SerialReceiveView::findNext(bool forward)
    {
        const QString keyword = m_searchEdit->text().trimmed();
        if (keyword.isEmpty())
        {
            return;
        }

        QTextDocument::FindFlags flags;
        if (!forward)
        {
            flags |= QTextDocument::FindBackward;
        }
        if (!m_textEdit->find(keyword, flags))
        {
            QTextCursor cursor = m_textEdit->textCursor();
            cursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
            m_textEdit->setTextCursor(cursor);
            m_textEdit->find(keyword, flags);
        }
    }

    void SerialReceiveView::applyHighlightSettings()
    {
        if (m_fixedFieldHighlighter == nullptr)
        {
            return;
        }
        m_fixedFieldHighlighter->setProperty("highlightEnabled", m_fixedFieldHighlightEnabled);
        m_textEdit->setProperty("directionBadgeEnabled", m_fixedFieldHighlightEnabled);
        m_fixedFieldHighlighter->rehighlight();
        m_textEdit->viewport()->update();
    }

} // namespace est
