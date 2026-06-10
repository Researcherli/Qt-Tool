#include "widgets/TerminalWidget.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBlock>
#include <QVBoxLayout>

namespace est
{

    // ---------------------------------------------------------------------------
    // ANSI 颜色表
    // ---------------------------------------------------------------------------

    namespace
    {
        /// 标准 ANSI 前景色（30-37）— 浅色主题优化
        const QVector<QColor> kAnsiFgStd = {
            QColor(QStringLiteral("#000000")), // 30 黑
            QColor(QStringLiteral("#C62828")), // 31 红
            QColor(QStringLiteral("#2E7D32")), // 32 绿
            QColor(QStringLiteral("#F57F17")), // 33 黄
            QColor(QStringLiteral("#1565C0")), // 34 蓝
            QColor(QStringLiteral("#6A1B9A")), // 35 紫
            QColor(QStringLiteral("#00838F")), // 36 青
            QColor(QStringLiteral("#546E7A")), // 37 白
        };

        /// 亮色 ANSI 前景色（90-97）
        const QVector<QColor> kAnsiFgBright = {
            QColor(QStringLiteral("#37474F")), // 90 亮黑
            QColor(QStringLiteral("#E53935")), // 91 亮红
            QColor(QStringLiteral("#43A047")), // 92 亮绿
            QColor(QStringLiteral("#FFB300")), // 93 亮黄
            QColor(QStringLiteral("#1E88E5")), // 94 亮蓝
            QColor(QStringLiteral("#8E24AA")), // 95 亮紫
            QColor(QStringLiteral("#00ACC1")), // 96 亮青
            QColor(QStringLiteral("#EEEEEC")), // 97 亮白
        };

        /// 标准 ANSI 背景色（40-47）
        const QVector<QColor> kAnsiBgStd = {
            QColor(QStringLiteral("#000000")),
            QColor(QStringLiteral("#CC0000")),
            QColor(QStringLiteral("#4E9A06")),
            QColor(QStringLiteral("#C4A000")),
            QColor(QStringLiteral("#3465A4")),
            QColor(QStringLiteral("#75507B")),
            QColor(QStringLiteral("#06989A")),
            QColor(QStringLiteral("#D3D7CF")),
        };

        /// 默认终端颜色（浅色主题）
        const QColor kDefaultFg(0x33, 0x33, 0x33); // 深灰
        const QColor kDefaultBg(0xFF, 0xFF, 0xFF); // 白色背景
        const QColor kSysMsgColor(0x88, 0x88, 0x88); // 系统消息灰

    } // namespace

    // ---------------------------------------------------------------------------
    // TerminalWidget
    // ---------------------------------------------------------------------------

    TerminalWidget::TerminalWidget(QWidget *parent)
        : QWidget(parent)
    {
        setupUi();
    }

    TerminalWidget::~TerminalWidget()
    {
        if (m_subscription.isValid() && m_dataBus)
            m_dataBus->unsubscribe(m_subscription);
    }

    void TerminalWidget::setupUi()
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        // 终端输出
        m_output = new QPlainTextEdit(this);
        m_output->setObjectName(QStringLiteral("terminalOutput"));
        m_output->setReadOnly(true);
        m_output->setUndoRedoEnabled(false);
        m_output->setMaximumBlockCount(kMaxLines);
        m_output->setStyleSheet(
            QStringLiteral("QPlainTextEdit {"
                           "  background: #FAFAFA;"
                           "  color: #333333;"
                           "  font-family: 'Consolas', 'Courier New', monospace;"
                           "  font-size: 13px;"
                           "  padding: 8px 10px;"
                           "  border: 1px solid #E0E0E0;"
                           "  selection-background-color: #BBDEFB;"
                           "}"));

        // 设置默认字体
        QFont terminalFont(QStringLiteral("Consolas"), 10);
        terminalFont.setStyleHint(QFont::Monospace);
        m_output->setFont(terminalFont);

        layout->addWidget(m_output, 1);

        // 输入行
        auto *inputLayout = new QHBoxLayout();
        inputLayout->setContentsMargins(0, 0, 0, 0);
        inputLayout->setSpacing(0);

        m_input = new QLineEdit(this);
        m_input->setObjectName(QStringLiteral("terminalInput"));
        m_input->setPlaceholderText(QStringLiteral("输入命令..."));
        m_input->setStyleSheet(
            QStringLiteral("QLineEdit {"
                           "  background: #FFFFFF;"
                           "  color: #333333;"
                           "  font-family: 'Consolas', 'Courier New', monospace;"
                           "  font-size: 13px;"
                           "  padding: 6px 10px;"
                           "  border: 1px solid #BDBDBD;"
                           "  border-top: 1px solid #E0E0E0;"
                           "}"
                           "QLineEdit:focus {"
                           "  border-color: #2F7FB5;"
                           "}"));
        m_input->setFont(terminalFont);
        inputLayout->addWidget(m_input, 1);

        m_sendBtn = new QPushButton(QStringLiteral("发送"), this);
        m_sendBtn->setObjectName(QStringLiteral("terminalSendBtn"));
        m_sendBtn->setFixedWidth(60);
        m_sendBtn->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: #0E639C;"
                           "  color: #FFF;"
                           "  border: none;"
                           "  padding: 6px 12px;"
                           "  font-size: 12px;"
                           "}"
                           "QPushButton:hover { background: #1177BB; }"
                           "QPushButton:pressed { background: #0D5689; }"));
        inputLayout->addWidget(m_sendBtn);

        layout->addLayout(inputLayout);

        // 信号连接
        connect(m_input, &QLineEdit::returnPressed,
                this, &TerminalWidget::onReturnPressed);
        connect(m_sendBtn, &QPushButton::clicked,
                this, &TerminalWidget::onReturnPressed);

        // 安装事件过滤器拦截方向键
        m_input->installEventFilter(this);
    }

    // ---------------------------------------------------------------------------
    // 输入处理
    // ---------------------------------------------------------------------------

    void TerminalWidget::onReturnPressed()
    {
        const QString text = m_input->text().trimmed();
        if (text.isEmpty())
            return;

        m_input->clear();

        // 在终端回显输入
        appendFormatted(m_prompt + text, QColor(QStringLiteral("#A0A0A0")),
                        QColor(), false);

        // 加入历史
        addHistory(text);

        // 发射信号
        emit lineSubmitted(text);
    }

    void TerminalWidget::addHistory(const QString &line)
    {
        // 不重复添加相同的最后一条
        if (!m_history.isEmpty() && m_history.last() == line)
            return;

        m_history.append(line);
        if (m_history.size() > kMaxHistory)
            m_history.removeFirst();

        m_historyIndex = m_history.size();
    }

    void TerminalWidget::updateHistory(int direction)
    {
        // direction: -1 = up, 1 = down
        if (m_history.isEmpty())
            return;

        m_historyIndex += direction;
        m_historyIndex = qBound(0, m_historyIndex, m_history.size());

        if (m_historyIndex < m_history.size())
            m_input->setText(m_history[m_historyIndex]);
        else
            m_input->clear();
    }

    bool TerminalWidget::handleKeyEvent(int key)
    {
        if (key == Qt::Key_Up)
        {
            updateHistory(-1);
            return true;
        }
        if (key == Qt::Key_Down)
        {
            updateHistory(1);
            return true;
        }
        return false;
    }

    bool TerminalWidget::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == m_input && event->type() == QEvent::KeyPress)
        {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (handleKeyEvent(keyEvent->key()))
                return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    // ---------------------------------------------------------------------------
    // 数据接收
    // ---------------------------------------------------------------------------

    void TerminalWidget::setDataBus(DataBus *bus)
    {
        if (m_dataBus)
        {
            if (m_subscription.isValid())
                m_dataBus->unsubscribe(m_subscription);
        }

        m_dataBus = bus;
        if (m_dataBus == nullptr)
            return;

        m_subscription = m_dataBus->subscribe(
            QStringLiteral("transport.serial.*"),
            [this](const DataPacket &packet)
            {
                onDataReceived(packet);
            });
    }

    void TerminalWidget::onDataReceived(const DataPacket &packet)
    {
        const QString text = QString::fromUtf8(packet.rawPayload);
        if (text.isEmpty())
            return;

        // 在主线程更新
        QMetaObject::invokeMethod(this, [this, text]()
                                  { processAnsiAndAppend(text); },
                                  Qt::QueuedConnection);
    }

    // ---------------------------------------------------------------------------
    // 输出
    // ---------------------------------------------------------------------------

    void TerminalWidget::write(const QString &text)
    {
        processAnsiAndAppend(text);
    }

    void TerminalWidget::appendReceived(const QByteArray &data)
    {
        const QString text = QString::fromUtf8(data);
        if (!text.isEmpty())
            processAnsiAndAppend(text);
    }

    void TerminalWidget::writeSystem(const QString &text)
    {
        appendFormatted(QStringLiteral("[系统] %1").arg(text),
                        kSysMsgColor, QColor(), true);
    }

    void TerminalWidget::clearTerminal()
    {
        m_output->clear();
    }

    void TerminalWidget::setConnected(bool connected)
    {
        m_connected = connected;
        m_prompt = connected ? QStringLiteral("> ") : QStringLiteral("(未连接) > ");
    }

    QString TerminalWidget::inputText() const
    {
        return m_input->text();
    }

    void TerminalWidget::setInputText(const QString &text)
    {
        m_input->setText(text);
    }

    // ---------------------------------------------------------------------------
    // ANSI 解析
    // ---------------------------------------------------------------------------

    void TerminalWidget::processAnsiAndAppend(const QString &text)
    {
        // ANSI 转义序列正则: \e\[(参数)m
        static const QRegularExpression s_ansi(
            QStringLiteral("\x1B\\[([0-9;]*)m"));

        int lastEnd = 0;
        QColor currentFg = kDefaultFg;
        QColor currentBg = kDefaultBg;
        bool currentBold = false;

        QRegularExpressionMatchIterator it = s_ansi.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch m = it.next();

            // 输出匹配前的普通文本
            if (m.capturedStart() > lastEnd)
            {
                const QString plain = text.mid(lastEnd, m.capturedStart() - lastEnd);
                appendFormatted(plain, currentFg, currentBg, currentBold);
            }

            // 解析 ANSI 参数
            const QString params = m.captured(1);
            if (params.isEmpty() || params == QStringLiteral("0"))
            {
                // 重置
                currentFg = kDefaultFg;
                currentBg = QColor();
                currentBold = false;
            }
            else
            {
                const QStringList codes = params.split(QChar(';'));
                for (const QString &codeStr : codes)
                {
                    bool ok = false;
                    const int code = codeStr.toInt(&ok);
                    if (!ok)
                        continue;

                    if (code == 1)
                        currentBold = true;
                    else if (code >= 30 && code <= 37)
                        currentFg = ansiFgColor(code - 30);
                    else if (code >= 90 && code <= 97)
                        currentFg = ansiFgColor(code - 90 + 8);
                    else if (code >= 40 && code <= 47)
                        currentBg = ansiBgColor(code - 40);
                    else if (code >= 100 && code <= 107)
                        currentBg = ansiBgColor(code - 100 + 8);
                }
            }

            lastEnd = m.capturedEnd();
        }

        // 输出剩余文本
        if (lastEnd < text.length())
        {
            const QString remaining = text.mid(lastEnd);
            appendFormatted(remaining, currentFg, currentBg, currentBold);
        }

        // 自动滚动到底部
        QScrollBar *sb = m_output->verticalScrollBar();
        if (sb)
            sb->setValue(sb->maximum());
    }

    void TerminalWidget::appendFormatted(const QString &text,
                                          const QColor &fg,
                                          const QColor &bg,
                                          bool bold)
    {
        if (text.isEmpty())
            return;

        QTextCharFormat fmt;
        fmt.setForeground(fg.isValid() ? fg : kDefaultFg);

        if (bg.isValid())
            fmt.setBackground(bg);

        if (bold)
            fmt.setFontWeight(QFont::Bold);

        // 等宽字体
        fmt.setFontFamily(QStringLiteral("Consolas"));
        fmt.setFontFixedPitch(true);

        QTextCursor cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(text, fmt);
    }

    // ---------------------------------------------------------------------------
    // ANSI 颜色查找
    // ---------------------------------------------------------------------------

    QColor TerminalWidget::ansiFgColor(int code) const
    {
        if (code >= 0 && code < 8)
            return kAnsiFgStd[code];
        if (code >= 8 && code < 16)
            return kAnsiFgBright[code - 8];
        return kDefaultFg;
    }

    QColor TerminalWidget::ansiBgColor(int code) const
    {
        if (code >= 0 && code < 8)
            return kAnsiBgStd[code];
        return QColor();
    }

} // namespace est
