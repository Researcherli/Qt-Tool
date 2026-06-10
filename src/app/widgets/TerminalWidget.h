#ifndef EST_TERMINALWIDGET_H
#define EST_TERMINALWIDGET_H

#include <QColor>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include "databus/DataPacket.h"
#include "databus/SubscriptionHandle.h"

class QEvent;
class QPlainTextEdit;
class QPushButton;

namespace est
{

    class DataBus;
    struct DataPacket;

    /**
     * 终端显示控件 — 类 PuTTY 风格的串口终端。
     *
     * 功能：
     *   - 分离式输入：上方终端输出(QPlainTextEdit) + 底部输入行(QLineEdit)
     *   - ANSI 彩色转义码解析（基础色）
     *   - 命令历史（Up/Down 导航）
     *   - DataBus 订阅接收串口数据
     */
    class TerminalWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit TerminalWidget(QWidget *parent = nullptr);
        ~TerminalWidget() override;

        /// 设置 DataBus（订阅 serial 通道接收数据）
        void setDataBus(DataBus *bus);

        /// 输出文本到终端（带 ANSI 解析）
        void write(const QString &text);

        /// 输出系统消息（灰色斜体）
        void writeSystem(const QString &text);

        /// 追加接收到的串口数据（带 ANSI 解析）
        void appendReceived(const QByteArray &data);

        /// 清空终端
        void clearTerminal();

        /// 获取输入行的当前文本
        QString inputText() const;

        /// 设置输入行文本
        void setInputText(const QString &text);

        /// 获取终端输出控件的指针（供脚本面板共享）
        QPlainTextEdit *outputWidget() const { return m_output; }

        /// 设置是否已连接（改变提示符颜色）
        void setConnected(bool connected);

        /// 向子控件转发按键事件
        bool handleKeyEvent(int key);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    signals:
        /// 用户按回车时发送数据
        void lineSubmitted(const QString &text);

        /// 请求发送原始字节
        void rawDataToSend(const QByteArray &data);

    private slots:
        void onReturnPressed();
        void onDataReceived(const DataPacket &packet);

    private:
        void setupUi();
        void processAnsiAndAppend(const QString &text);
        void appendFormatted(const QString &text, const QColor &fg,
                              const QColor &bg, bool bold);
        void appendPrompt();
        void updateHistory(int direction);
        void addHistory(const QString &line);

        // ANSI 颜色映射
        QColor ansiFgColor(int code) const;
        QColor ansiBgColor(int code) const;

        static const int kMaxLines = 10000;
        static const int kMaxHistory = 100;

        QPlainTextEdit *m_output = nullptr;
        QLineEdit *m_input = nullptr;
        QPushButton *m_sendBtn = nullptr;

        DataBus *m_dataBus = nullptr;
        SubscriptionHandle m_subscription;

        // 命令历史
        QVector<QString> m_history;
        int m_historyIndex = -1;

        // 连接状态
        bool m_connected = false;
        QString m_prompt = QStringLiteral("(未连接) > ");
    };

} // namespace est

#endif // EST_TERMINALWIDGET_H
