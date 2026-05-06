#include "pages/SerialAssistantPage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "plugin/ICore.h"
#include "services/RecentRecordManager.h"
#include "transport/ITransport.h"
#include "transport/TransportRegistry.h"
#include "widgets/QuickCommandPanel.h"
#include "widgets/SerialConfigBar.h"
#include "widgets/SerialReceiveView.h"
#include "widgets/SerialSendPanel.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <QSerialPortInfo>

namespace est
{

    SerialAssistantPage::SerialAssistantPage(ICore *core,
                                             QWidget *parent)
        : QWidget(parent)
        , m_dataBus(core->dataBus())
        , m_transportRegistry(core->transportRegistry())
        , m_recentRecordManager(core->recentRecordManager())
    {
        setObjectName(QStringLiteral("serialAssistantPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(8);

        m_configBar = new SerialConfigBar(this);
        auto *contentWidget = new QWidget(this);
        auto *contentLayout = new QHBoxLayout(contentWidget);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(8);

        auto *mainColumnWidget = new QWidget(contentWidget);
        auto *mainColumnLayout = new QVBoxLayout(mainColumnWidget);
        mainColumnLayout->setContentsMargins(0, 0, 0, 0);
        mainColumnLayout->setSpacing(8);

        m_receiveView = new SerialReceiveView(mainColumnWidget);
        m_sendPanel = new SerialSendPanel(mainColumnWidget);
        m_quickCommandPanel = new QuickCommandPanel(contentWidget);
        auto *rightColumnWidget = new QWidget(contentWidget);
        rightColumnWidget->setObjectName(QStringLiteral("serialRightColumn"));
        auto *rightColumnLayout = new QVBoxLayout(rightColumnWidget);
        rightColumnLayout->setContentsMargins(0, 0, 0, 0);
        rightColumnLayout->setSpacing(8);

        auto *rightOptionPanel = new QWidget(rightColumnWidget);
        rightOptionPanel->setObjectName(QStringLiteral("serialRightOptionPanel"));
        rightOptionPanel->setMinimumHeight(252);
        rightOptionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto *rightOptionLayout = new QVBoxLayout(rightOptionPanel);
        rightOptionLayout->setContentsMargins(0, 0, 0, 0);
        rightOptionLayout->setSpacing(0);

        auto *rightSettingsPanel = new QWidget(rightColumnWidget);
        rightSettingsPanel->setObjectName(QStringLiteral("serialRightSettingsPanel"));
        rightSettingsPanel->setMinimumHeight(156);
        rightSettingsPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        auto *rightSettingsLayout = new QVBoxLayout(rightSettingsPanel);
        rightSettingsLayout->setContentsMargins(0, 0, 0, 0);
        rightSettingsLayout->setSpacing(0);

        m_quickCommandPanel->setMinimumWidth(360);
        m_quickCommandPanel->setMaximumWidth(360);
        rightColumnWidget->setMinimumWidth(360);
        rightColumnWidget->setMaximumWidth(360);
        mainColumnLayout->addWidget(m_receiveView, 1);
        mainColumnLayout->addWidget(m_sendPanel);
        m_sendPanel->moveControlsTo(rightSettingsPanel);
        m_sendPanel->moveOptionsTo(rightOptionPanel);
        rightColumnLayout->addWidget(m_quickCommandPanel, 10);
        rightColumnLayout->addWidget(rightOptionPanel);
        rightColumnLayout->addWidget(rightSettingsPanel, 1);
        contentLayout->addWidget(mainColumnWidget, 1);
        contentLayout->addWidget(rightColumnWidget);

        rootLayout->addWidget(m_configBar);
        rootLayout->addWidget(contentWidget, 1);

        m_autoSendTimer = new QTimer(this);
        m_autoSendTimer->setTimerType(Qt::CoarseTimer);


        loadSettings();
        m_receiveView->setTimestampVisible(m_sendPanel->timestampEnabled());
        refreshPorts();
        m_configBar->setConnectionState(false, tr("未连接"), QStringLiteral("disconnected"));
        emit transferStatsChanged(m_txBytes, m_rxBytes);

        connect(m_configBar, &SerialConfigBar::refreshRequested, this, &SerialAssistantPage::refreshPorts);
        const auto showSerialSettings = [this]() {
            if (m_configBar->showAdvancedSettingsDialog())
            {
                saveSettings();
                emit systemLogGenerated(tr("串口高级设置已更新：%1").arg(m_configBar->dataFormatSummary()));
            }
        };
        connect(m_configBar, &SerialConfigBar::settingsRequested, this, showSerialSettings);
        connect(m_configBar, &SerialConfigBar::openRequested, this, &SerialAssistantPage::openCurrentPort);
        connect(m_configBar, &SerialConfigBar::closeRequested, this, &SerialAssistantPage::closeCurrentPort);
        connect(m_configBar, &SerialConfigBar::settingsChanged, this, &SerialAssistantPage::saveSettings);
        connect(m_sendPanel, &SerialSendPanel::sendRequested, this, &SerialAssistantPage::sendCurrentPayload);
        connect(m_sendPanel, &SerialSendPanel::clearDataRequested, this, &SerialAssistantPage::clearTransferData);
        connect(m_sendPanel, &SerialSendPanel::saveCommandRequested, this, &SerialAssistantPage::saveDraftAsQuickCommand);
        connect(m_sendPanel, &SerialSendPanel::timestampDisplayChanged, m_receiveView, &SerialReceiveView::setTimestampVisible);
        connect(m_sendPanel, &SerialSendPanel::settingsChanged, this, [this]() {
            saveSettings();
            updateAutoSendState();
        });
        connect(m_quickCommandPanel, &QuickCommandPanel::commandTriggered, this, &SerialAssistantPage::sendCommand);
        connect(m_quickCommandPanel, &QuickCommandPanel::statusMessageGenerated, this, &SerialAssistantPage::systemLogGenerated);
        connect(m_receiveView, &SerialReceiveView::statusMessageGenerated, this, &SerialAssistantPage::systemLogGenerated);
        connect(m_receiveView, &SerialReceiveView::entriesCleared, this, [this]() {
            m_txBytes = 0;
            m_rxBytes = 0;
            emit transferStatsChanged(m_txBytes, m_rxBytes);
        });
        connect(m_autoSendTimer, &QTimer::timeout, this, &SerialAssistantPage::sendCurrentPayload);
    }

    SerialAssistantPage::~SerialAssistantPage()
    {
        if (m_autoSendTimer != nullptr)
        {
            m_autoSendTimer->stop();
        }
        saveSettings();
        if (m_transport != nullptr)
        {
            m_transport->close();
        }
    }

    void SerialAssistantPage::refreshPorts()
    {
        const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        QList<QPair<QString, QString>> items;
        items.reserve(ports.size());

        if (ports.isEmpty())
        {
            m_configBar->setPortItems({}, m_lastPortName);
            m_configBar->setConnectionState(false, tr("未检测到可用串口"), QStringLiteral("warning"));
            emit systemLogGenerated(tr("刷新串口列表完成，未发现可用串口"));
            saveSettings();
            return;
        }

        for (const QSerialPortInfo &info : ports)
        {
            const QString label = info.description().isEmpty()
                                      ? info.portName()
                                      : tr("%1 - %2").arg(info.portName(), info.description());
            items.append({label, info.portName()});
        }

        m_configBar->setPortItems(items, m_lastPortName);
        saveSettings();
        emit systemLogGenerated(tr("刷新串口列表完成，发现 %1 个端口").arg(ports.size()));
    }

    bool SerialAssistantPage::ensureTransport()
    {
        if (m_transport != nullptr)
        {
            return true;
        }

        if (m_transportRegistry == nullptr || !m_transportRegistry->hasType(QStringLiteral("serial")))
        {
            m_configBar->setConnectionState(false, tr("串口传输未注册"), QStringLiteral("error"));
            emit systemLogGenerated(tr("串口传输工厂未注册，无法创建 SerialTransport"));
            return false;
        }

        m_transport = m_transportRegistry->createTransport(QStringLiteral("serial"));
        if (m_transport == nullptr)
        {
            m_configBar->setConnectionState(false, tr("串口传输创建失败"), QStringLiteral("error"));
            emit systemLogGenerated(tr("创建串口传输实例失败"));
            return false;
        }

        m_transport->setParent(this);

        connect(m_transport, &ITransport::dataReceived, this, [this](const DataPacket &packet)
                {
        handlePacketReceived(packet); });

        connect(m_transport, &ITransport::stateChanged, this, [this](ITransport::State state)
                {
        switch (state) {
        case ITransport::State::Disconnected:
            m_configBar->setConnectionState(false, tr("未连接"), QStringLiteral("disconnected"));
            m_sendPanel->setConnected(false);
            emit serialStatusChanged(tr("未连接"), false);
            if (m_autoSendTimer != nullptr) {
                m_autoSendTimer->stop();
            }
            break;
        case ITransport::State::Connecting:
            m_configBar->setConnectionState(false, tr("正在连接 %1").arg(m_lastPortName), QStringLiteral("warning"));
            emit serialStatusChanged(tr("连接中 %1").arg(m_lastPortName), false);
            break;
        case ITransport::State::Connected:
            m_configBar->setConnectionState(true, tr("%1 已连接").arg(m_lastPortName), QStringLiteral("connected"));
            m_sendPanel->setConnected(true);
            emit serialStatusChanged(tr("已连接 %1").arg(m_lastPortName), true);
            emit systemLogGenerated(tr("串口已连接：%1").arg(m_lastPortName));
            m_recentRecordManager->addSerialProfile(m_lastPortName,
                                                    m_configBar->currentConfig().value(QStringLiteral("baudRate")).toInt(),
                                                    m_configBar->dataFormatSummary());
            emit recentRecordsChanged();
            updateAutoSendState();
            break;
        case ITransport::State::Error:
            m_configBar->setConnectionState(false, tr("连接异常"), QStringLiteral("error"));
            m_sendPanel->setConnected(false);
            emit serialStatusChanged(tr("连接异常"), false);
            if (m_autoSendTimer != nullptr) {
                m_autoSendTimer->stop();
            }
            break;
        } });

        connect(m_transport, &ITransport::errorOccurred, this, [this](const QString &errorMessage)
                {
        m_configBar->setConnectionState(false, tr("错误：%1").arg(errorMessage), QStringLiteral("error"));
        QMessageBox::warning(this, tr("串口错误"), errorMessage);
        emit systemLogGenerated(tr("串口错误：%1").arg(errorMessage)); });

        return true;
    }

    void SerialAssistantPage::openCurrentPort()
    {
        if (!ensureTransport())
        {
            return;
        }

        const QVariantMap config = m_configBar->currentConfig();
        m_lastPortName = config.value(QStringLiteral("portName")).toString();
        if (m_lastPortName.isEmpty())
        {
            QMessageBox::warning(this, tr("无法打开串口"), tr("请先选择一个可用串口。"));
            m_configBar->setConnectionState(false, tr("未选择串口"), QStringLiteral("warning"));
            return;
        }

        emit systemLogGenerated(tr("尝试打开串口：%1").arg(m_lastPortName));
        if (!m_transport->open(config))
        {
            m_sendPanel->setConnected(false);
            emit serialStatusChanged(tr("未连接"), false);
        }
        else
        {
            saveSettings();
        }
    }

    void SerialAssistantPage::closeCurrentPort()
    {
        if (m_transport != nullptr)
        {
            m_transport->close();
            emit systemLogGenerated(tr("已关闭串口：%1").arg(m_lastPortName));
        }
    }

    void SerialAssistantPage::handlePacketReceived(const DataPacket &packet)
    {
        if (m_dataBus != nullptr)
        {
            m_dataBus->publish(packet.channel, packet);
        }
        m_receiveView->appendPacket(packet);
        m_rxBytes += packet.rawPayload.size();
        emit transferStatsChanged(m_txBytes, m_rxBytes);
    }

    void SerialAssistantPage::sendCurrentPayload()
    {
        if (m_transport == nullptr || m_transport->state() != ITransport::State::Connected)
        {
            m_configBar->setConnectionState(false, tr("串口未连接，无法发送"), QStringLiteral("warning"));
            emit systemLogGenerated(tr("发送失败：当前没有打开的串口"));
            if (m_autoSendTimer != nullptr)
            {
                m_autoSendTimer->stop();
            }
            return;
        }

        QString errorMessage;
        const QByteArray payload = m_sendPanel->buildPayload(&errorMessage);
        if (!errorMessage.isEmpty())
        {
            QMessageBox::warning(this, tr("发送失败"), errorMessage);
            emit systemLogGenerated(errorMessage);
            return;
        }

        DataPacket packet;
        packet.timestamp = QDateTime::currentMSecsSinceEpoch();
        packet.channel = QStringLiteral("transport.serial.%1").arg(m_lastPortName);
        packet.rawPayload = payload;
        packet.metadata.insert(QStringLiteral("direction"), QStringLiteral("tx"));
        packet.metadata.insert(QStringLiteral("portName"), m_lastPortName);

        if (!m_transport->send(packet))
        {
            m_configBar->setConnectionState(true, tr("发送失败"), QStringLiteral("error"));
            emit systemLogGenerated(tr("发送失败：无法写入串口 %1").arg(m_lastPortName));
            return;
        }

        if (m_dataBus != nullptr)
        {
            m_dataBus->publish(packet.channel, packet);
        }
        m_receiveView->appendPacket(packet);

        m_txBytes += payload.size();
        emit transferStatsChanged(m_txBytes, m_rxBytes);
        m_configBar->setConnectionState(true, tr("%1 已连接").arg(m_lastPortName), QStringLiteral("connected"));
        emit systemLogGenerated(tr("已发送 %1 字节到 %2").arg(payload.size()).arg(m_lastPortName));
        saveSettings();
    }

    void SerialAssistantPage::clearTransferData()
    {
        m_receiveView->clearEntries();
    }

    void SerialAssistantPage::updateAutoSendState()
    {
        if (m_autoSendTimer == nullptr)
        {
            return;
        }

        const bool shouldRun = m_sendPanel->autoSendEnabled()
                               && m_transport != nullptr
                               && m_transport->state() == ITransport::State::Connected;

        if (!shouldRun)
        {
            m_autoSendTimer->stop();
            return;
        }

        m_autoSendTimer->start(m_sendPanel->autoSendIntervalMs());
    }

    void SerialAssistantPage::loadSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("serialAssistant"));
        m_lastPortName = settings.value(QStringLiteral("portName")).toString();
        m_configBar->applySettings(settings.value(QStringLiteral("configBar")).toMap());
        m_sendPanel->applySettings(settings.value(QStringLiteral("sendPanel")).toMap());

        settings.endGroup();
    }

    void SerialAssistantPage::saveSettings() const
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("serialAssistant"));
        settings.setValue(QStringLiteral("portName"), m_configBar->currentPortName());
        settings.setValue(QStringLiteral("configBar"), m_configBar->savedSettings());
        settings.setValue(QStringLiteral("sendPanel"), m_sendPanel->savedSettings());
        settings.endGroup();
    }

    void SerialAssistantPage::sendCommand(const est::QuickCommandDefinition &command)
    {
        m_sendPanel->setCommandDraft(command);
        sendCurrentPayload();
    }

    void SerialAssistantPage::saveDraftAsQuickCommand()
    {
        m_quickCommandPanel->promptAddCommand(m_sendPanel->draftCommand());
    }

} // namespace est
