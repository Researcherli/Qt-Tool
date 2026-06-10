#ifndef EST_SERIALCONSOLEPAGE_H
#define EST_SERIALCONSOLEPAGE_H

#include <QProcess>
#include <QVBoxLayout>
#include <QWidget>

#include "databus/SubscriptionHandle.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QTextEdit;
class QLabel;
class QProcess;
class QPushButton;
class QTabWidget;
class QTextEdit;

namespace est
{

    class CodeEditor;
    class DataBus;
    class ICore;
    class ITransport;
    class TerminalWidget;
    class TransportRegistry;
    struct DataPacket;

    /**
     * 终端控制台页面 — 串口终端 + Python 脚本自动化。
     *
     * 包含两个标签页：
     *   - 终端：PuTTY 风格串口终端（ANSI 颜色 + 命令历史）
     *   - 脚本：CodeEditor 编辑 Python 脚本 + 子进程执行
     */
    class SerialConsolePage : public QWidget
    {
        Q_OBJECT

    public:
        explicit SerialConsolePage(ICore *core, QWidget *parent = nullptr);
        ~SerialConsolePage() override;

    signals:
        void statusMessageGenerated(const QString &text);

    private slots:
        void onConnectToggle();
        void onTerminalLineSubmitted(const QString &text);
        void onRunScript();
        void onStopScript();
        void onSaveScript();
        void onLoadScript();
        void onPythonStdout();
        void onPythonStderr();
        void onPythonFinished(int exitCode, QProcess::ExitStatus status);
        void onSerialDataForScript(const QString &payload);

    private:
        void setupConnectionBar(QVBoxLayout *root);
        void setupTerminalTab();
        void setupScriptTab();
        void refreshPorts();
        bool ensureTransport();
        void closeCurrentTransport();
        void updateConnectionUi(bool connected, const QString &text, const QString &color);
        void handlePacketReceived(const DataPacket &packet);
        void sendRawData(const QByteArray &data);
        void sendHexData(const QString &hexStr);
        void startPythonProcess();
        void stopPythonProcess();
        void sendToPython(const QByteArray &jsonLine);
        QString findPython() const;

        // Core
        DataBus *m_dataBus = nullptr;
        TransportRegistry *m_transportRegistry = nullptr;
        ICore *m_core = nullptr;
        ITransport *m_transport = nullptr;
        QString m_lastPortName;

        // 连接栏
        QComboBox *m_portCombo = nullptr;
        QComboBox *m_baudCombo = nullptr;
        QPushButton *m_connectBtn = nullptr;
        QLabel *m_connStatus = nullptr;

        // 标签
        QTabWidget *m_tabWidget = nullptr;

        // 终端标签
        TerminalWidget *m_terminal = nullptr;

        // 脚本标签
        CodeEditor *m_scriptEditor = nullptr;
        TerminalWidget *m_scriptTerminal = nullptr;
        QPushButton *m_runBtn = nullptr;
        QPushButton *m_stopBtn = nullptr;
        QPushButton *m_saveBtn = nullptr;
        QPushButton *m_loadBtn = nullptr;

        // Python 子进程
        QProcess *m_pythonProc = nullptr;
        QByteArray m_pythonStdoutBuf;
        bool m_scriptRunning = false;
        bool m_pythonReady = false;
    };

} // namespace est

#endif // EST_SERIALCONSOLEPAGE_H
