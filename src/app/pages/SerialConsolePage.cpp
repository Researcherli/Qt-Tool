#include "pages/SerialConsolePage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "pages/SerialPortEnumerator.h"
#include "plugin/ICore.h"
#include "services/AppPaths.h"
#include "transport/ITransport.h"
#include "transport/TransportRegistry.h"
#include "widgets/CodeEditor.h"
#include "widgets/TerminalWidget.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace est
{

    SerialConsolePage::SerialConsolePage(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_dataBus(core ? core->dataBus() : nullptr)
        , m_transportRegistry(core ? core->transportRegistry() : nullptr)
        , m_core(core)
    {
        setObjectName(QStringLiteral("serialConsolePage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        setupConnectionBar(rootLayout);
        setupTerminalTab();
        setupScriptTab();
    }

    SerialConsolePage::~SerialConsolePage()
    {
        stopPythonProcess();
        closeCurrentTransport();
    }

    // ---------------------------------------------------------------------------
    // UI 构建
    // ---------------------------------------------------------------------------

    void SerialConsolePage::setupConnectionBar(QVBoxLayout *root)
    {
        auto *bar = new QWidget(this);
        bar->setObjectName(QStringLiteral("consoleConnectionBar"));
        bar->setStyleSheet(
            QStringLiteral("QWidget#consoleConnectionBar {"
                           "  background: #F5F5F5;"
                           "  border-bottom: 1px solid #E0E0E0;"
                           "  padding: 6px 12px;"
                           "}"));

        auto *layout = new QHBoxLayout(bar);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(8);

        auto *portLabel = new QLabel(tr("串口:"), bar);
        portLabel->setStyleSheet(QStringLiteral("color: #555; font-size: 12px;"));
        layout->addWidget(portLabel);

        m_portCombo = new QComboBox(bar);
        m_portCombo->setMinimumWidth(120);
        // 端口在打开时刷新
        layout->addWidget(m_portCombo);

        auto *baudLabel = new QLabel(tr("波特率:"), bar);
        baudLabel->setStyleSheet(QStringLiteral("color: #CCC; font-size: 12px;"));
        layout->addWidget(baudLabel);

        m_baudCombo = new QComboBox(bar);
        m_baudCombo->addItems({
            QStringLiteral("9600"), QStringLiteral("19200"),
            QStringLiteral("38400"), QStringLiteral("57600"),
            QStringLiteral("115200"), QStringLiteral("230400"),
            QStringLiteral("460800"), QStringLiteral("921600"),
        });
        m_baudCombo->setCurrentText(QStringLiteral("115200"));
        layout->addWidget(m_baudCombo);

        layout->addStretch(1);

        m_connStatus = new QLabel(tr("未连接"), bar);
        m_connStatus->setStyleSheet(QStringLiteral("color: #F44336; font-size: 12px;"));
        layout->addWidget(m_connStatus);

        m_connectBtn = new QPushButton(tr("⚡打开"), bar);
        m_connectBtn->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: #0E639C; color: #FFF;"
                           "  border: none; padding: 5px 16px;"
                           "  font-size: 12px; border-radius: 3px;"
                           "}"
                           "QPushButton:hover { background: #1177BB; }"
                           "QPushButton:checked { background: #D32F2F; }"));
        m_connectBtn->setCheckable(true);
        layout->addWidget(m_connectBtn);

        root->addWidget(bar);

        connect(m_connectBtn, &QPushButton::clicked,
                this, &SerialConsolePage::onConnectToggle);

        QTimer::singleShot(0, this, &SerialConsolePage::refreshPorts);
    }

    void SerialConsolePage::setupTerminalTab()
    {
        if (!m_tabWidget)
        {
            m_tabWidget = new QTabWidget(this);
            m_tabWidget->setStyleSheet(
                QStringLiteral("QTabWidget::pane {"
                               "  border: none;"
                               "  background: #FFFFFF;"
                               "}"
                               "QTabBar::tab {"
                               "  background: #E8E8E8; color: #555;"
                               "  padding: 6px 20px;"
                               "  border: none;"
                               "  font-size: 12px;"
                               "}"
                               "QTabBar::tab:selected {"
                               "  background: #FFFFFF;"
                               "  color: #333;"
                               "  border-bottom: 2px solid #2F7FB5;"
                               "}"
                               "QTabBar::tab:hover {"
                               "  background: #F0F0F0;"
                               "}"));

            auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
            if (rootLayout)
                rootLayout->addWidget(m_tabWidget, 1);
        }

        // 终端标签
        m_terminal = new TerminalWidget(this);

        connect(m_terminal, &TerminalWidget::lineSubmitted,
                this, &SerialConsolePage::onTerminalLineSubmitted);

        // 启用 TerminalWidget 的 DataBus 订阅，接收回放数据
        if (m_dataBus != nullptr)
        {
            m_terminal->setDataBus(m_dataBus);
        }

        m_tabWidget->addTab(m_terminal, tr("终端"));
    }

    void SerialConsolePage::setupScriptTab()
    {
        // 脚本标签 — 包含脚本面板
        auto *scriptPage = new QWidget(this);
        auto *scriptLayout = new QVBoxLayout(scriptPage);
        scriptLayout->setContentsMargins(0, 0, 0, 0);
        scriptLayout->setSpacing(0);

        // 脚本工具栏
        auto *scriptToolbar = new QWidget(scriptPage);
        scriptToolbar->setStyleSheet(
            QStringLiteral("background: #F5F5F5; padding: 4px 8px;"
                           "border-bottom: 1px solid #E0E0E0;"));
        auto *tbLayout = new QHBoxLayout(scriptToolbar);
        tbLayout->setContentsMargins(4, 4, 4, 4);
        tbLayout->setSpacing(6);

        m_runBtn = new QPushButton(tr("▶ 运行"), scriptToolbar);
        m_runBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #388E3C; color: #FFF;"
                           "  border: none; padding: 4px 14px; font-size: 12px;"
                           "  border-radius: 3px; }"
                           "QPushButton:hover { background: #43A047; }"));
        tbLayout->addWidget(m_runBtn);

        m_stopBtn = new QPushButton(tr("⏹ 停止"), scriptToolbar);
        m_stopBtn->setEnabled(false);
        m_stopBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #D32F2F; color: #FFF;"
                           "  border: none; padding: 4px 14px; font-size: 12px;"
                           "  border-radius: 3px; }"
                           "QPushButton:hover { background: #E53935; }"
                           "QPushButton:disabled { background: #BDBDBD; }"));
        tbLayout->addWidget(m_stopBtn);

        m_saveBtn = new QPushButton(tr("💾 保存"), scriptToolbar);
        m_saveBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #1565C0; color: #FFF;"
                           "  border: none; padding: 4px 14px; font-size: 12px;"
                           "  border-radius: 3px; }"
                           "QPushButton:hover { background: #1976D2; }"));
        tbLayout->addWidget(m_saveBtn);

        m_loadBtn = new QPushButton(tr("📂 加载"), scriptToolbar);
        m_loadBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #6A1B9A; color: #FFF;"
                           "  border: none; padding: 4px 14px; font-size: 12px;"
                           "  border-radius: 3px; }"
                           "QPushButton:hover { background: #7B1FA2; }"));
        tbLayout->addWidget(m_loadBtn);

        tbLayout->addStretch(1);

        scriptLayout->addWidget(scriptToolbar);

        // 拆分器: 编辑器 + 终端
        auto *splitter = new QSplitter(Qt::Horizontal, scriptPage);

        m_scriptEditor = new CodeEditor(splitter);
        m_scriptEditor->setStyleSheet(
            QStringLiteral("QPlainTextEdit {"
                           "  background: #FFFFFF; color: #333333;"
                           "  font-family: 'Consolas', 'Courier New', monospace;"
                           "  font-size: 13px;"
                           "  border: none;"
                           "}"));

        m_scriptTerminal = new TerminalWidget(splitter);
        m_scriptTerminal->setStyleSheet(QString()); // 移除额外样式

        // 设置默认脚本内容
        m_scriptEditor->setPlainText(
            QStringLiteral("# Python 串口自动化脚本\n"
                           "# 可用函数: send(), send_hex(), wait(), print(), delay()\n"
                           "#\n"
                           "# print(\"=== 开始测试 ===\")\n"
                           "# send(\"AT\\r\\n\")\n"
                           "# r = wait(\"OK\", 3000)\n"
                           "# print(r)\n"));

        splitter->addWidget(m_scriptEditor);
        splitter->addWidget(m_scriptTerminal);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);

        scriptLayout->addWidget(splitter, 1);

        m_tabWidget->addTab(scriptPage, tr("脚本"));

        // 连接
        connect(m_runBtn, &QPushButton::clicked,
                this, &SerialConsolePage::onRunScript);
        connect(m_stopBtn, &QPushButton::clicked,
                this, &SerialConsolePage::onStopScript);
        connect(m_saveBtn, &QPushButton::clicked,
                this, &SerialConsolePage::onSaveScript);
        connect(m_loadBtn, &QPushButton::clicked,
                this, &SerialConsolePage::onLoadScript);
    }

    // ---------------------------------------------------------------------------
    // 串口连接
    // ---------------------------------------------------------------------------

    void SerialConsolePage::refreshPorts()
    {
        const QString previousPort = m_portCombo->currentData().toString().isEmpty()
                                         ? m_portCombo->currentText()
                                         : m_portCombo->currentData().toString();
        m_portCombo->clear();

        const QList<QSerialPortInfo> ports = safeAvailablePorts();
        for (const QSerialPortInfo &info : ports)
        {
            const QString label = info.description().isEmpty()
                                      ? info.portName()
                                      : tr("%1 - %2").arg(info.portName(), info.description());
            m_portCombo->addItem(label, info.portName());
        }

        const QString preferred = previousPort.isEmpty() ? m_lastPortName : previousPort;
        const int index = m_portCombo->findData(preferred);
        if (index >= 0)
        {
            m_portCombo->setCurrentIndex(index);
        }

        if (ports.isEmpty())
        {
            updateConnectionUi(false, tr("未检测到串口"), QStringLiteral("#FF9800"));
        }
    }

    bool SerialConsolePage::ensureTransport()
    {
        if (m_transport != nullptr)
        {
            return true;
        }

        if (m_transportRegistry == nullptr || !m_transportRegistry->hasType(QStringLiteral("serial")))
        {
            updateConnectionUi(false, tr("串口传输未注册"), QStringLiteral("#F44336"));
            emit statusMessageGenerated(tr("终端控制台无法创建串口传输：serial 未注册"));
            return false;
        }

        m_transport = m_transportRegistry->createTransport(QStringLiteral("serial"));
        if (m_transport == nullptr)
        {
            updateConnectionUi(false, tr("串口传输创建失败"), QStringLiteral("#F44336"));
            emit statusMessageGenerated(tr("终端控制台串口传输创建失败"));
            return false;
        }

        m_transport->setParent(this);

        connect(m_transport, &ITransport::dataReceived,
                this, &SerialConsolePage::handlePacketReceived);

        connect(m_transport, &ITransport::stateChanged,
                this, [this](ITransport::State state)
                {
                    switch (state)
                    {
                    case ITransport::State::Connected:
                        updateConnectionUi(true, tr("%1 已连接").arg(m_lastPortName), QStringLiteral("#4CAF50"));
                        m_terminal->writeSystem(tr("串口已打开：%1 @ %2 baud").arg(m_lastPortName, m_baudCombo->currentText()));
                        emit statusMessageGenerated(tr("终端串口已连接：%1").arg(m_lastPortName));
                        break;
                    case ITransport::State::Connecting:
                        updateConnectionUi(false, tr("正在连接 %1").arg(m_lastPortName), QStringLiteral("#FF9800"));
                        break;
                    case ITransport::State::Error:
                        updateConnectionUi(false, tr("连接异常"), QStringLiteral("#F44336"));
                        m_connectBtn->setChecked(false);
                        break;
                    case ITransport::State::Disconnected:
                        updateConnectionUi(false, tr("未连接"), QStringLiteral("#F44336"));
                        m_connectBtn->setChecked(false);
                        emit statusMessageGenerated(tr("终端串口已断开"));
                        break;
                    }
                });

        connect(m_transport, &ITransport::errorOccurred,
                this, [this](const QString &message)
                {
                    updateConnectionUi(false, tr("错误：%1").arg(message), QStringLiteral("#F44336"));
                    m_terminal->writeSystem(tr("串口错误：%1").arg(message));
                    emit statusMessageGenerated(tr("终端串口错误：%1").arg(message));
                });

        return true;
    }

    void SerialConsolePage::closeCurrentTransport()
    {
        if (m_transport == nullptr)
            return;

        if (m_transport->state() == ITransport::State::Connected)
        {
            m_transport->close();
        }
        m_transport->deleteLater();
        m_transport = nullptr;
    }

    void SerialConsolePage::updateConnectionUi(bool connected, const QString &text, const QString &color)
    {
        m_connStatus->setText(text);
        m_connStatus->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(color));
        m_terminal->setConnected(connected);
        m_connectBtn->setText(connected ? tr("关闭") : tr("打开"));
    }

    void SerialConsolePage::handlePacketReceived(const DataPacket &packet)
    {
        if (m_dataBus != nullptr)
        {
            m_dataBus->publish(packet.channel, packet);
        }

        // 实时数据直接显示到终端（回放数据通过 TerminalWidget 的 DataBus 订阅显示）
        m_terminal->appendReceived(packet.rawPayload);
        onSerialDataForScript(QString::fromUtf8(packet.rawPayload));
    }

    void SerialConsolePage::onConnectToggle()
    {
        if (m_transport != nullptr && m_transport->state() == ITransport::State::Connected)
        {
            closeCurrentTransport();
            updateConnectionUi(false, tr("未连接"), QStringLiteral("#F44336"));
            m_terminal->writeSystem(tr("串口已关闭"));
            return;
        }

        refreshPorts();

        const QString portName = m_portCombo->currentData().toString().isEmpty()
                                     ? m_portCombo->currentText().trimmed()
                                     : m_portCombo->currentData().toString();
        if (portName.isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("请选择串口号"));
            m_connectBtn->setChecked(false);
            return;
        }

        if (!ensureTransport())
        {
            m_connectBtn->setChecked(false);
            return;
        }

        m_lastPortName = portName;

        QVariantMap config;
        config[QStringLiteral("portName")] = portName;
        config[QStringLiteral("baudRate")] = m_baudCombo->currentText().toInt();
        config[QStringLiteral("dataBits")] = 8;
        config[QStringLiteral("parity")] = QStringLiteral("none");
        config[QStringLiteral("stopBits")] = QStringLiteral("1");

        if (!m_transport->open(config))
        {
            QMessageBox::warning(this, tr("打开失败"), tr("无法打开串口 %1").arg(portName));
            closeCurrentTransport();
            updateConnectionUi(false, tr("未连接"), QStringLiteral("#F44336"));
            m_connectBtn->setChecked(false);
        }
    }

    void SerialConsolePage::onTerminalLineSubmitted(const QString &text)
    {
        // 发送到串口
        const QByteArray payload = text.toUtf8() + "\r\n";
        sendRawData(payload);

        // 回显到终端（TerminalWidget 已完成回显）
    }

    void SerialConsolePage::sendRawData(const QByteArray &data)
    {
        if (m_transport != nullptr && m_transport->state() == ITransport::State::Connected)
        {
            DataPacket packet;
            packet.channel = QStringLiteral("transport.serial.%1").arg(m_lastPortName);
            packet.rawPayload = data;
            packet.timestamp = QDateTime::currentMSecsSinceEpoch();
            packet.metadata.insert(QStringLiteral("direction"), QStringLiteral("tx"));
            packet.metadata.insert(QStringLiteral("portName"), m_lastPortName);

            if (!m_transport->send(packet))
            {
                m_terminal->writeSystem(tr("发送失败：串口未写入"));
                return;
            }

            if (m_dataBus != nullptr)
            {
                m_dataBus->publish(packet.channel, packet);
            }
            return;
        }

        m_terminal->writeSystem(tr("发送失败：请先打开串口"));
    }

    void SerialConsolePage::sendHexData(const QString &hexStr)
    {
        const QByteArray data = QByteArray::fromHex(hexStr.toLatin1());
        sendRawData(data);
    }

    void SerialConsolePage::onSerialDataForScript(const QString &payload)
    {
        if (m_pythonProc && m_pythonProc->state() == QProcess::Running)
        {
            QJsonObject msg;
            msg[QStringLiteral("cmd")] = QStringLiteral("data");
            msg[QStringLiteral("payload")] = payload;
            sendToPython(QJsonDocument(msg).toJson(QJsonDocument::Compact));
        }
    }

    // ---------------------------------------------------------------------------
    // Python 脚本
    // ---------------------------------------------------------------------------

    QString SerialConsolePage::findPython() const
    {
        // 按优先级查找
        const QStringList candidates = {
            QStringLiteral("python3"),
            QStringLiteral("python"),
            QStringLiteral("py"),
        };

        for (const auto &c : candidates)
        {
            const QString executable = QStandardPaths::findExecutable(c);
            if (executable.isEmpty())
                continue;

            QProcess test;
            test.start(executable, {QStringLiteral("--version")});
            if (test.waitForFinished(3000) && test.exitCode() == 0)
                return executable;
        }

        return QStringLiteral("python");
    }

    void SerialConsolePage::startPythonProcess()
    {
        if (m_pythonProc)
        {
            if (m_pythonProc->state() != QProcess::NotRunning)
                return;
            delete m_pythonProc;
        }

        m_pythonProc = new QProcess(this);
        m_pythonStdoutBuf.clear();
        m_pythonReady = false;

        connect(m_pythonProc, &QProcess::readyReadStandardOutput,
                this, &SerialConsolePage::onPythonStdout);
        connect(m_pythonProc, &QProcess::readyReadStandardError,
                this, &SerialConsolePage::onPythonStderr);
        connect(m_pythonProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &SerialConsolePage::onPythonFinished);

        // 从 QRC 提取助手脚本到临时文件
        QFile qrcFile(QStringLiteral(":/scripts/est_serial_helper.py"));
        if (!qrcFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            m_scriptTerminal->writeSystem(
                tr("错误：无法加载脚本引擎"));
            return;
        }

        const QString helperContent = QString::fromUtf8(qrcFile.readAll());
        qrcFile.close();

        // 写临时文件（使用 PID 后缀避免多实例竞争）
        const QString tmpPath = QDir::tempPath()
            + QStringLiteral("/est_serial_helper_%1.py").arg(QCoreApplication::applicationPid());
        QFile tmpFile(tmpPath);
        if (tmpFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&tmpFile);
            out << helperContent;
            tmpFile.close();
        }

        const QString pythonExe = findPython();
        m_pythonProc->start(pythonExe, {tmpPath});

        if (!m_pythonProc->waitForStarted(5000))
        {
            m_scriptTerminal->writeSystem(
                tr("错误：无法启动 Python 进程 (%1)").arg(pythonExe));
            delete m_pythonProc;
            m_pythonProc = nullptr;
            return;
        }

        m_scriptTerminal->writeSystem(
            tr("Python 脚本引擎已启动"));

        // 等待 ready 事件
        if (!m_pythonProc->waitForReadyRead(3000))
        {
            m_scriptTerminal->writeSystem(
                tr("警告：脚本引擎启动可能异常"));
        }
    }

    void SerialConsolePage::stopPythonProcess()
    {
        if (m_pythonProc && m_pythonProc->state() != QProcess::NotRunning)
        {
            sendToPython(QByteArrayLiteral("{\"cmd\":\"shutdown\"}\n"));
            if (!m_pythonProc->waitForFinished(3000))
            {
                m_pythonProc->kill();
                m_pythonProc->waitForFinished(1000);
            }
        }

        m_scriptRunning = false;
        m_runBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
    }

    void SerialConsolePage::sendToPython(const QByteArray &jsonLine)
    {
        if (m_pythonProc && m_pythonProc->state() == QProcess::Running)
        {
            m_pythonProc->write(jsonLine + "\n");
            m_pythonProc->waitForBytesWritten(100);
        }
    }

    void SerialConsolePage::onPythonStdout()
    {
        m_pythonStdoutBuf.append(m_pythonProc->readAllStandardOutput());

        // 按行处理 JSON
        while (true)
        {
            const int newlinePos = m_pythonStdoutBuf.indexOf('\n');
            if (newlinePos < 0)
                break;

            const QByteArray line = m_pythonStdoutBuf.left(newlinePos).trimmed();
            m_pythonStdoutBuf.remove(0, newlinePos + 1);

            if (line.isEmpty())
                continue;

            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject())
                continue;

            const QJsonObject msg = doc.object();
            const QString type = msg.value(QStringLiteral("type")).toString();

            if (type == QStringLiteral("send"))
            {
                const QString data = msg.value(QStringLiteral("data")).toString();
                sendRawData(data.toUtf8());
            }
            else if (type == QStringLiteral("send_hex"))
            {
                const QString data = msg.value(QStringLiteral("data")).toString();
                sendHexData(data);
            }
            else if (type == QStringLiteral("print"))
            {
                const QString text = msg.value(QStringLiteral("msg")).toString();
                m_scriptTerminal->write(text + QStringLiteral("\n"));
            }
            else if (type == QStringLiteral("wait_start"))
            {
                m_scriptTerminal->writeSystem(
                    tr("等待数据：%1").arg(msg.value(QStringLiteral("pattern")).toString()));
            }
            else if (type == QStringLiteral("wait_result"))
            {
                if (!msg.value(QStringLiteral("found")).toBool())
                    m_scriptTerminal->writeSystem(tr("等待超时"));
            }
            else if (type == QStringLiteral("error"))
            {
                const QString errMsg = msg.value(QStringLiteral("msg")).toString();
                m_scriptTerminal->write(
                    QStringLiteral("\x1B[91m错误: %1\x1B[0m\n").arg(errMsg));
            }
            else if (type == QStringLiteral("finished"))
            {
                const bool success = msg.value(QStringLiteral("success")).toBool();
                m_scriptRunning = false;
                m_runBtn->setEnabled(true);
                m_stopBtn->setEnabled(false);
                if (success)
                    m_scriptTerminal->writeSystem(tr("脚本执行完成 ✓"));
                else
                    m_scriptTerminal->writeSystem(tr("脚本执行失败 ✗"));
            }
            else if (type == QStringLiteral("ready"))
            {
                m_pythonReady = true;
            }
        }
    }

    void SerialConsolePage::onPythonStderr()
    {
        const QByteArray err = m_pythonProc->readAllStandardError();
        if (!err.isEmpty())
        {
            m_scriptTerminal->write(
                QStringLiteral("\x1B[91m%1\x1B[0m\n")
                    .arg(QString::fromUtf8(err).trimmed()));
        }
    }

    void SerialConsolePage::onPythonFinished(int exitCode, QProcess::ExitStatus status)
    {
        m_scriptRunning = false;
        m_runBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);

        if (status == QProcess::CrashExit)
            m_scriptTerminal->writeSystem(tr("Python 进程异常退出"));
        else if (exitCode != 0)
            m_scriptTerminal->writeSystem(
                tr("Python 进程退出 (代码 %1)").arg(exitCode));
    }

    // ---------------------------------------------------------------------------
    // 脚本控制 Slot
    // ---------------------------------------------------------------------------

    void SerialConsolePage::onRunScript()
    {
        const QString script = m_scriptEditor->toPlainText().trimmed();
        if (script.isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("请输入脚本内容"));
            return;
        }

        // 确保 Python 进程运行
        if (!m_pythonProc || m_pythonProc->state() != QProcess::Running)
        {
            startPythonProcess();
        }

        if (!m_pythonProc || m_pythonProc->state() != QProcess::Running)
        {
            QMessageBox::warning(this, tr("错误"), tr("无法启动 Python 脚本引擎"));
            return;
        }

        m_scriptRunning = true;
        m_runBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_scriptTerminal->clearTerminal();
        m_scriptTerminal->writeSystem(tr("开始执行脚本..."));

        // 发送脚本到 Python 进程
        QJsonObject msg;
        msg[QStringLiteral("cmd")] = QStringLiteral("run");
        msg[QStringLiteral("script")] = script;
        sendToPython(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    }

    void SerialConsolePage::onStopScript()
    {
        if (m_pythonProc && m_pythonProc->state() == QProcess::Running)
        {
            sendToPython(QByteArrayLiteral("{\"cmd\":\"abort\"}\n"));
        }

        m_scriptRunning = false;
        m_runBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_scriptTerminal->writeSystem(tr("脚本已终止"));
    }

    void SerialConsolePage::onSaveScript()
    {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("保存脚本"),
            AppPaths::scriptFilePath(QStringLiteral("serial_script.py")),
            tr("Python 脚本 (*.py);;所有文件 (*)"));

        if (path.isEmpty())
            return;

        if (AppPaths::isDriveCPath(path))
        {
            emit statusMessageGenerated(tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
            return;
        }

        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out << m_scriptEditor->toPlainText();
            file.close();
            emit statusMessageGenerated(
                tr("脚本已保存: %1").arg(path));
        }
    }

    void SerialConsolePage::onLoadScript()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("加载脚本"),
            AppPaths::scriptsDir(),
            tr("Python 脚本 (*.py);;所有文件 (*)"));

        if (path.isEmpty())
            return;

        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            m_scriptEditor->setPlainText(in.readAll());
            file.close();
            emit statusMessageGenerated(
                tr("脚本已加载: %1").arg(path));
        }
    }

} // namespace est
