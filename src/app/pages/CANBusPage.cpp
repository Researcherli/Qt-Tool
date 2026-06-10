#include "pages/CANBusPage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "pages/SerialPortEnumerator.h"
#include "plugin/ICore.h"
#include "services/AppPaths.h"
#include "services/SlcanCodec.h"
#include "transport/ITransport.h"
#include "transport/SerialTransport.h"
#include "transport/TransportRegistry.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFocusEvent>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QStringConverter>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace est
{

    // ---------------------------------------------------------------------------
    // slcan 波特率映射
    // ---------------------------------------------------------------------------

    namespace
    {
        constexpr int kMaxFrameRows = 5000;

        class CanPortComboBox : public QComboBox
        {
        public:
            explicit CanPortComboBox(CANBusPage *page, QWidget *parent = nullptr)
                : QComboBox(parent)
                , m_page(page)
            {
                setProperty("popupPlacement", QStringLiteral("below"));
            }

            void showPopup() override
            {
                if (m_page != nullptr)
                {
                    QMetaObject::invokeMethod(m_page, "refreshPorts", Qt::QueuedConnection);
                }
                QComboBox::showPopup();

                QWidget *popupWindow = view() != nullptr ? view()->window() : nullptr;
                if (popupWindow == nullptr)
                {
                    return;
                }

                const QPoint popupTopLeft = mapToGlobal(QPoint(0, height()));
                popupWindow->move(popupTopLeft);
                if (popupWindow->width() < width())
                {
                    popupWindow->resize(width(), popupWindow->height());
                }
            }

        private:
            CANBusPage *m_page = nullptr;
        };

        class CanByteLineEdit : public QLineEdit
        {
        public:
            explicit CanByteLineEdit(QWidget *parent = nullptr)
                : QLineEdit(parent)
            {
            }

        protected:
            void focusInEvent(QFocusEvent *event) override
            {
                QLineEdit::focusInEvent(event);
                selectAll();
                m_selectOnMouseRelease = event->reason() == Qt::MouseFocusReason;
            }

            void mouseReleaseEvent(QMouseEvent *event) override
            {
                QLineEdit::mouseReleaseEvent(event);
                if (m_selectOnMouseRelease)
                {
                    selectAll();
                    m_selectOnMouseRelease = false;
                }
            }

        private:
            bool m_selectOnMouseRelease = false;
        };

        QString portSearchText(const QSerialPortInfo &port)
        {
            return QStringList({
                port.portName(),
                port.description(),
                port.manufacturer(),
                port.serialNumber(),
                port.systemLocation()
            }).join(QLatin1Char(' ')).toLower();
        }

        bool isKnownCanableVidPid(const QSerialPortInfo &port)
        {
            if (!port.hasVendorIdentifier() || !port.hasProductIdentifier())
            {
                return false;
            }

            const quint16 vid = port.vendorIdentifier();
            const quint16 pid = port.productIdentifier();
            return (vid == 0x1D50 && pid == 0x606F)  // candleLight / CANable-compatible
                || (vid == 0x1209 && pid == 0x0001) // common CANable firmware builds
                || (vid == 0x16D0 && pid == 0x117E); // CANable 2.0 style community VID/PID
        }

        bool isCanablePort(const QSerialPortInfo &port)
        {
            if (isKnownCanableVidPid(port))
            {
                return true;
            }

            const QString text = portSearchText(port);
            return text.contains(QStringLiteral("canable"))
                || text.contains(QStringLiteral("candlelight"))
                || text.contains(QStringLiteral("cantact"))
                || text.contains(QStringLiteral("slcan"))
                || text.contains(QStringLiteral("lawicel"));
        }

        QString canPortDisplayName(const QSerialPortInfo &port)
        {
            const QString description = port.description().trimmed();
            if (description.isEmpty())
            {
                return port.portName();
            }
            return QStringLiteral("%1 - %2").arg(port.portName(), description);
        }

        /// CAN 帧列索引
        enum FrameColumn
        {
            ColIndex = 0,
            ColTimestamp,
            ColDir,
            ColID,
            ColDLC,
            ColData,
            ColCount
        };

        enum FrameItemRole
        {
            CanIdRole = Qt::UserRole + 1,
            ExtendedRole
        };

        QString csvEscaped(QString text)
        {
            text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            return QStringLiteral("\"%1\"").arg(text);
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // CANBusPage
    // ---------------------------------------------------------------------------

    CANBusPage::CANBusPage(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_core(core)
        , m_dataBus(core ? core->dataBus() : nullptr)
    {
        setObjectName(QStringLiteral("canBusPage"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(8);

        setupConnectionBar(root);
        setupSendPanel(root);
        setupReceiveTable(root);

        m_periodicTimer = new QTimer(this);
        connect(m_periodicTimer, &QTimer::timeout, this, [this]() {
            sendCurrentFrame(true);
        });

        // 订阅 DataBus 接收回放数据（实时数据通过 ITransport 直连显示）
        if (m_dataBus != nullptr)
        {
            m_subscription = m_dataBus->subscribe(
                QStringLiteral("transport.can.*"),
                [this](const DataPacket &packet)
                {
                    // 仅处理回放数据，避免实时模式下重复显示
                    if (!packet.metadata.value(QStringLiteral("est.replayed")).toBool())
                        return;

                    uint32_t canId = packet.metadata.value(QStringLiteral("can_id")).toULongLong();
                    int dlc = packet.metadata.value(QStringLiteral("dlc")).toInt();
                    bool extended = packet.metadata.value(QStringLiteral("extended")).toBool();
                    bool isTx = packet.metadata.value(QStringLiteral("direction")) == QStringLiteral("tx");
                    addFrameToTable(canId, dlc, packet.rawPayload, extended, isTx);
                });
        }

        refreshPorts();
    }

    CANBusPage::~CANBusPage()
    {
        closeCANable();
    }

    // ---------------------------------------------------------------------------
    // UI 构建
    // ---------------------------------------------------------------------------

    void CANBusPage::setupConnectionBar(QVBoxLayout *root)
    {
        auto *bar = new QWidget(this);
        auto *layout = new QHBoxLayout(bar);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto *portLabel = new QLabel(tr("串口:"), bar);
        layout->addWidget(portLabel);

        m_portCombo = new CanPortComboBox(this, bar);
        m_portCombo->setMinimumWidth(220);
        m_portCombo->setEditable(false);
        m_portCombo->setObjectName(QStringLiteral("canPortCombo"));
        auto *portView = new QListView();
        portView->setSpacing(4);
        m_portCombo->setView(portView);
        layout->addWidget(m_portCombo);

        m_refreshPortsBtn = new QPushButton(tr("刷新"), bar);
        m_refreshPortsBtn->setObjectName(QStringLiteral("canRefreshPortsButton"));
        layout->addWidget(m_refreshPortsBtn);

        auto *baudLabel = new QLabel(tr("CAN 波特率:"), bar);
        layout->addWidget(baudLabel);

        m_baudRateCombo = new QComboBox(bar);
        m_baudRateCombo->addItem(QStringLiteral("10 kbit/s"), 10000);
        m_baudRateCombo->addItem(QStringLiteral("20 kbit/s"), 20000);
        m_baudRateCombo->addItem(QStringLiteral("50 kbit/s"), 50000);
        m_baudRateCombo->addItem(QStringLiteral("100 kbit/s"), 100000);
        m_baudRateCombo->addItem(QStringLiteral("125 kbit/s"), 125000);
        m_baudRateCombo->addItem(QStringLiteral("250 kbit/s"), 250000);
        m_baudRateCombo->addItem(QStringLiteral("500 kbit/s"), 500000);
        m_baudRateCombo->addItem(QStringLiteral("800 kbit/s"), 800000);
        m_baudRateCombo->addItem(QStringLiteral("1 Mbit/s"), 1000000);
        m_baudRateCombo->setCurrentIndex(6); // 默认 500k
        layout->addWidget(m_baudRateCombo);

        layout->addStretch(1);

        m_statusLabel = new QLabel(tr("未连接"), bar);
        m_statusLabel->setStyleSheet(QStringLiteral("color: #F44336; font-weight: bold;"));
        layout->addWidget(m_statusLabel);

        m_connectBtn = new QPushButton(tr("⚡打开 CAN"), bar);
        m_connectBtn->setCheckable(true);
        m_connectBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #0E639C; color: #FFF;"
                           "  border: none; padding: 5px 16px;"
                           "  border-radius: 3px; font-size: 12px; }"
                           "QPushButton:hover { background: #1177BB; }"
                           "QPushButton:checked { background: #D32F2F; }"));
        layout->addWidget(m_connectBtn);

        root->addWidget(bar);

        connect(m_connectBtn, &QPushButton::toggled,
                this, &CANBusPage::onConnectToggle);
        connect(m_refreshPortsBtn, &QPushButton::clicked,
                this, &CANBusPage::refreshPorts);
    }

    void CANBusPage::setupSendPanel(QVBoxLayout *root)
    {
        auto *group = new QGroupBox(tr("发送 CAN 帧"), this);
        auto *layout = new QHBoxLayout(group);
        layout->setSpacing(8);

        auto *idLabel = new QLabel(tr("ID (Hex):"), group);
        layout->addWidget(idLabel);

        m_idEdit = new QLineEdit(group);
        m_idEdit->setPlaceholderText(QStringLiteral("123"));
        m_idEdit->setFixedWidth(80);
        m_idEdit->setMaxLength(8);
        m_idEdit->setObjectName(QStringLiteral("canIdEdit"));
        layout->addWidget(m_idEdit);

        m_idTypeCombo = new QComboBox(group);
        m_idTypeCombo->addItem(tr("标准帧 11位"));
        m_idTypeCombo->addItem(tr("扩展帧 29位"));
        layout->addWidget(m_idTypeCombo);

        auto *dlcLabel = new QLabel(tr("DLC:"), group);
        layout->addWidget(dlcLabel);

        m_dlcSpin = new QSpinBox(group);
        m_dlcSpin->setRange(0, 8);
        m_dlcSpin->setValue(0);
        m_dlcSpin->setFixedWidth(50);
        m_dlcSpin->setObjectName(QStringLiteral("canDlcSpin"));
        layout->addWidget(m_dlcSpin);

        auto *dataLabel = new QLabel(tr("数据 (Hex):"), group);
        layout->addWidget(dataLabel);

        auto *dataBytesWidget = new QWidget(group);
        dataBytesWidget->setObjectName(QStringLiteral("canDataEdit"));
        auto *dataBytesLayout = new QHBoxLayout(dataBytesWidget);
        dataBytesLayout->setContentsMargins(0, 0, 0, 0);
        dataBytesLayout->setSpacing(4);
        m_dataByteEdits.clear();
        for (int index = 0; index < 8; ++index)
        {
            auto *byteEdit = new CanByteLineEdit(dataBytesWidget);
            byteEdit->setObjectName(QStringLiteral("canDataByte%1").arg(index));
            byteEdit->setMaxLength(2);
            byteEdit->setFixedWidth(38);
            byteEdit->setAlignment(Qt::AlignCenter);
            byteEdit->setPlaceholderText(QStringLiteral("00"));
            byteEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9A-Fa-f]{0,2}")), byteEdit));
            byteEdit->setEnabled(false);
            QFont byteFont(QStringLiteral("Consolas"), 10);
            byteFont.setBold(true);
            byteEdit->setFont(byteFont);
            connect(byteEdit, &QLineEdit::editingFinished, this, [this, byteEdit]() {
                QString text = byteEdit->text().trimmed().toUpper();
                byteEdit->setText(text);
            });
            m_dataByteEdits.append(byteEdit);
            dataBytesLayout->addWidget(byteEdit);
        }
        layout->addWidget(dataBytesWidget);

        m_periodicCheck = new QCheckBox(tr("周期"), group);
        m_periodicCheck->setObjectName(QStringLiteral("canPeriodicCheck"));
        layout->addWidget(m_periodicCheck);

        m_periodSpin = new QSpinBox(group);
        m_periodSpin->setRange(10, 60000);
        m_periodSpin->setValue(1000);
        m_periodSpin->setSuffix(tr(" ms"));
        m_periodSpin->setFixedWidth(112);
        m_periodSpin->setEnabled(false);
        m_periodSpin->setObjectName(QStringLiteral("canPeriodSpin"));
        layout->addWidget(m_periodSpin);

        m_sendBtn = new QPushButton(tr("发送"), group);
        m_sendBtn->setObjectName(QStringLiteral("canSendButton"));
        m_sendBtn->setStyleSheet(
            QStringLiteral("QPushButton { background: #388E3C; color: #FFF;"
                           "  border: none; padding: 5px 20px;"
                           "  border-radius: 3px; font-weight: bold; }"
                           "QPushButton:hover { background: #43A047; }"
                           "QPushButton:disabled { background: #BDBDBD; }"));
        m_sendBtn->setEnabled(false);
        layout->addWidget(m_sendBtn);

        root->addWidget(group);

        connect(m_sendBtn, &QPushButton::clicked, this, &CANBusPage::onSend);
        connect(m_periodicCheck, &QCheckBox::toggled,
                this, &CANBusPage::onPeriodicToggled);
        connect(m_periodSpin, qOverload<int>(&QSpinBox::valueChanged),
                this, [this](int value) {
                    if (m_periodicTimer != nullptr && m_periodicTimer->isActive())
                    {
                        m_periodicTimer->setInterval(value);
                    }
                });
        connect(m_dlcSpin, qOverload<int>(&QSpinBox::valueChanged),
                this, &CANBusPage::updatePayloadEditorsFromDlc);
        updatePayloadEditorsFromDlc();
    }

    void CANBusPage::setupReceiveTable(QVBoxLayout *root)
    {
        auto *bar = new QWidget(this);
        auto *barLayout = new QHBoxLayout(bar);
        barLayout->setContentsMargins(0, 0, 0, 0);

        auto *titleLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("CAN 数据")), bar);
        barLayout->addWidget(titleLabel);

        barLayout->addStretch(1);

        m_statsLabel = new QLabel(tr("共 0 帧"), bar);
        barLayout->addWidget(m_statsLabel);

        m_rxOverwriteCheck = new QCheckBox(tr("按 ID 覆盖"), bar);
        m_rxOverwriteCheck->setObjectName(QStringLiteral("canRxOverwriteCheck"));
        m_rxOverwriteCheck->setToolTip(tr("开启后，同一 CAN ID 的接收帧只刷新同一行；关闭后每帧滚动追加。"));
        barLayout->addWidget(m_rxOverwriteCheck);

        m_saveRxBtn = new QPushButton(tr("保存 RX"), bar);
        m_saveRxBtn->setObjectName(QStringLiteral("canSaveRxButton"));
        m_saveRxBtn->setEnabled(false);
        m_saveRxBtn->setToolTip(tr("将全部 RX 接收历史保存为 CSV 文件。"));
        barLayout->addWidget(m_saveRxBtn);

        m_clearBtn = new QPushButton(tr("清空"), bar);
        barLayout->addWidget(m_clearBtn);

        root->addWidget(bar);

        auto *tableLayout = new QHBoxLayout();
        tableLayout->setContentsMargins(0, 0, 0, 0);
        tableLayout->setSpacing(8);

        auto *txGroup = new QGroupBox(tr("发送 TX"), this);
        auto *txLayout = new QVBoxLayout(txGroup);
        txLayout->setContentsMargins(6, 8, 6, 6);
        m_txFrameTable = createFrameTable(txGroup);
        m_txFrameTable->setObjectName(QStringLiteral("canTxFrameTable"));
        txLayout->addWidget(m_txFrameTable);

        auto *rxGroup = new QGroupBox(tr("接收 RX"), this);
        auto *rxLayout = new QVBoxLayout(rxGroup);
        rxLayout->setContentsMargins(6, 8, 6, 6);
        m_rxFrameTable = createFrameTable(rxGroup);
        m_rxFrameTable->setObjectName(QStringLiteral("canRxFrameTable"));
        QFont rxFont = m_rxFrameTable->font();
        rxFont.setPointSize(rxFont.pointSize() > 0 ? rxFont.pointSize() + 2 : 12);
        m_rxFrameTable->setFont(rxFont);
        m_rxFrameTable->verticalHeader()->setDefaultSectionSize(30);
        rxLayout->addWidget(m_rxFrameTable);

        tableLayout->addWidget(txGroup, 1);
        tableLayout->addWidget(rxGroup, 1);
        root->addLayout(tableLayout, 1);

        connect(m_clearBtn, &QPushButton::clicked, this, &CANBusPage::onClear);
        connect(m_saveRxBtn, &QPushButton::clicked, this, &CANBusPage::onSaveRxData);
        connect(m_rxOverwriteCheck, &QCheckBox::toggled,
                this, &CANBusPage::rebuildRxFrameTable);
    }

    QTableWidget *CANBusPage::createFrameTable(QWidget *parent)
    {
        auto *table = new QTableWidget(parent);
        table->setColumnCount(ColCount);
        table->setHorizontalHeaderLabels({
            tr("#"), tr("时间"), tr("方向"), tr("ID"), tr("DLC"), tr("数据")
        });
        table->horizontalHeader()->setSectionResizeMode(ColIndex, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(ColTimestamp, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(ColDir, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(ColID, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(ColDLC, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(ColData, QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(false);
        table->setShowGrid(true);
        table->setGridStyle(Qt::SolidLine);
        table->verticalHeader()->setDefaultSectionSize(34);
        table->verticalHeader()->setVisible(false);
        table->setStyleSheet(QStringLiteral(
            "QTableWidget {"
            "  background: #FFFFFF;"
            "  alternate-background-color: #FFFFFF;"
            "  gridline-color: #D7DEE8;"
            "  selection-background-color: #CFE8FF;"
            "  selection-color: #111827;"
            "}"
            "QHeaderView::section {"
            "  background: #E9EEF5;"
            "  color: #1F2937;"
            "  border: 0;"
            "  border-right: 1px solid #D0D8E4;"
            "  border-bottom: 1px solid #C9D3E0;"
            "  padding: 6px 8px;"
            "  font-weight: 700;"
            "}"));
        return table;
    }

    // ---------------------------------------------------------------------------
    // 连接管理
    // ---------------------------------------------------------------------------

    void CANBusPage::onConnectToggle()
    {
        if (m_closing)
        {
            return;
        }

        if (m_connectBtn->isChecked())
            openCANable();
        else
            closeCANable();
    }

    void CANBusPage::openCANable()
    {
        if (m_transport)
        {
            closeCANable();
        }

        const QString portName = currentPortName();
        if (portName.isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("未检测到 CANable 设备，请连接后刷新"));
            m_connectBtn->setChecked(false);
            return;
        }

        ITransport *transport = nullptr;
        if (m_core != nullptr && m_core->transportRegistry() != nullptr)
        {
            transport = m_core->transportRegistry()->createTransport(QStringLiteral("serial"));
        }
        if (transport == nullptr)
        {
            transport = new SerialTransport();
        }
        transport->setParent(this);

        QVariantMap config;
        config[QStringLiteral("portName")] = portName;
        config[QStringLiteral("baudRate")] = 115200; // CANable 固件固定 115200
        config[QStringLiteral("dataBits")] = 8;
        config[QStringLiteral("parity")] = QStringLiteral("none");
        config[QStringLiteral("stopBits")] = QStringLiteral("1");

        m_lastTransportError.clear();
        connect(transport, &ITransport::dataReceived,
                this, &CANBusPage::onTransportData);
        connect(transport, &ITransport::stateChanged,
                this, &CANBusPage::onTransportStateChanged);
        connect(transport, &ITransport::errorOccurred,
                this, &CANBusPage::onTransportError);

        if (!transport->open(config))
        {
            const QString detail = m_lastTransportError.isEmpty()
                ? tr("请检查设备是否已连接，或端口是否被其它软件占用")
                : m_lastTransportError;
            QMessageBox::warning(this, tr("打开失败"),
                tr("无法打开 CANable 串口 %1\n%2").arg(portName, detail));
            m_connectBtn->setChecked(false);
            transport->deleteLater();
            return;
        }

        m_transport = transport;
        m_connected = false;
        m_statusLabel->setText(tr("正在配置..."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #FF9800; font-weight: bold;"));

        const QByteArray bitrateCommand = SlcanCodec::bitrateCommand(selectedBitrate());
        if (bitrateCommand.isEmpty())
        {
            QMessageBox::warning(this, tr("提示"), tr("当前 CAN 波特率不受 SLCAN 支持"));
            closeCANable();
            m_connectBtn->setChecked(false);
            return;
        }

        sendSLCAN(QByteArrayLiteral("C\r"));
        QTimer::singleShot(30, this, [this, bitrateCommand]() {
            sendSLCAN(bitrateCommand + QByteArrayLiteral("\r"));
        });
        QTimer::singleShot(100, this, [this]()
                           {
            if (m_transport == nullptr || m_transport->state() != ITransport::State::Connected)
                return;
            sendSLCAN(QByteArrayLiteral("O\r")); // 打开 CAN
            m_connected = true;
            setConnectedUi(true, tr("CAN 已打开"));
            emit statusMessageGenerated(tr("CAN 已打开 @ %1").arg(m_baudRateCombo->currentText())); });
    }

    void CANBusPage::closeCANable()
    {
        m_closing = true;
        if (m_periodicTimer != nullptr)
        {
            m_periodicTimer->stop();
        }
        if (m_periodicCheck != nullptr)
        {
            m_periodicCheck->setChecked(false);
        }

        if (m_transport)
        {
            sendSLCAN(QByteArrayLiteral("C\r")); // 关闭 CAN
            m_transport->close();
            m_transport->deleteLater();
            m_transport = nullptr;
        }

        setConnectedUi(false, tr("未连接"));
        m_closing = false;
        emit statusMessageGenerated(tr("CAN 已关闭"));
    }

    void CANBusPage::sendSLCAN(const QByteArray &cmd)
    {
        if (!m_transport || m_transport->state() != ITransport::State::Connected)
            return;

        DataPacket pkt;
        pkt.channel = QStringLiteral("transport.can.slcan");
        pkt.rawPayload = cmd;
        pkt.timestamp = QDateTime::currentMSecsSinceEpoch();
        m_transport->send(pkt);
    }

    // ---------------------------------------------------------------------------
    // 数据传输
    // ---------------------------------------------------------------------------

    void CANBusPage::onTransportData(const DataPacket &packet)
    {
        parseIncomingData(packet.rawPayload);
    }

    void CANBusPage::onTransportStateChanged(ITransport::State state)
    {
        if (state == ITransport::State::Disconnected ||
            state == ITransport::State::Error)
        {
            if (!m_closing && m_connectBtn->isChecked())
            {
                m_connectBtn->setChecked(false);
            }
        }
    }

    void CANBusPage::onTransportError(const QString &message)
    {
        m_lastTransportError = message;
        if (!message.isEmpty())
        {
            emit statusMessageGenerated(tr("CAN 串口错误：%1").arg(message));
        }
    }

    void CANBusPage::parseIncomingData(const QByteArray &data)
    {
        m_lineBuffer += QString::fromLatin1(data);

        // 按行处理（slcan 协议以 \r 结尾）
        while (true)
        {
            int idx = m_lineBuffer.indexOf(QChar('\r'));
            if (idx < 0)
                break;

            const QString line = m_lineBuffer.left(idx).trimmed();
            m_lineBuffer.remove(0, idx + 1);

            if (line.isEmpty())
                continue;

            CanFrame frame;
            if (SlcanCodec::parseFrameLine(line, &frame))
            {
                addFrameToTable(frame.id, frame.dlc, frame.data, frame.extended, false);
                publishFrame(frame.id, frame.dlc, frame.data, frame.extended, false);
                continue;
            }

            if (line == QStringLiteral("\a"))
            {
                emit statusMessageGenerated(tr("CAN 适配器返回错误响应"));
            }
        }
    }

    void CANBusPage::onSend()
    {
        sendCurrentFrame(false);
    }

    void CANBusPage::sendCurrentFrame(bool fromPeriodic)
    {
        if (!m_transport || m_transport->state() != ITransport::State::Connected || !m_connected)
        {
            if (!fromPeriodic)
            {
                QMessageBox::warning(this, tr("提示"), tr("请先打开 CAN"));
            }
            return;
        }

        bool ok = false;
        const QString idStr = m_idEdit->text().trimmed();
        if (idStr.isEmpty())
        {
            if (!fromPeriodic)
            {
                QMessageBox::warning(this, tr("提示"), tr("请输入 CAN ID"));
            }
            return;
        }

        uint32_t id = idStr.toUInt(&ok, 16);
        if (!ok)
        {
            if (!fromPeriodic)
            {
                QMessageBox::warning(this, tr("提示"), tr("ID 格式无效，请输入十六进制"));
            }
            return;
        }

        const bool extended = (m_idTypeCombo->currentIndex() == 1);
        const int dlc = m_dlcSpin->value();
        QByteArray dataBytes;
        QString errorMessage;
        for (int index = 0; index < dlc; ++index)
        {
            if (index >= m_dataByteEdits.size())
            {
                errorMessage = tr("数据字节输入框数量不足");
                break;
            }

            QLineEdit *byteEdit = m_dataByteEdits.at(index);
            QString text = byteEdit->text().trimmed().toUpper();
            if (text.isEmpty())
            {
                text = QStringLiteral("00");
            }
            if (text.size() == 1)
            {
                text.prepend(QLatin1Char('0'));
            }
            byteEdit->setText(text);

            bool byteOk = false;
            const uint value = text.toUInt(&byteOk, 16);
            if (!byteOk || value > 0xFF)
            {
                errorMessage = tr("第 %1 个数据字节无效").arg(index + 1);
                break;
            }
            dataBytes.append(static_cast<char>(value));
        }

        if (!errorMessage.isEmpty()
            || !SlcanCodec::validateDataFrame(id, dlc, dataBytes, extended, &errorMessage))
        {
            if (!fromPeriodic)
            {
                QMessageBox::warning(this, tr("提示"), errorMessage);
            }
            else
            {
                m_periodicTimer->stop();
                m_periodicCheck->setChecked(false);
                emit statusMessageGenerated(tr("周期发送已停止：%1").arg(errorMessage));
            }
            return;
        }

        sendSLCAN(SlcanCodec::buildDataFrame(id, dlc, dataBytes, extended));

        addFrameToTable(id, dlc, dataBytes, extended, true);
        publishFrame(id, dlc, dataBytes, extended, true);
    }

    // ---------------------------------------------------------------------------
    // 辅助方法
    // ---------------------------------------------------------------------------

    void CANBusPage::addFrameToTable(uint32_t id, int dlc,
                                      const QByteArray &data,
                                      bool extended, bool isTx)
    {
        QTableWidget *table = isTx ? m_txFrameTable : m_rxFrameTable;
        if (table == nullptr)
        {
            return;
        }

        m_frameCount++;
        if (isTx)
            ++m_txCount;
        else
            ++m_rxCount;

        CanFrameRecord record;
        record.sequence = isTx ? m_txCount : m_rxCount;
        record.timestamp = QDateTime::currentMSecsSinceEpoch();
        record.id = id;
        record.dlc = dlc;
        record.data = data;
        record.extended = extended;
        record.isTx = isTx;

        if (!isTx)
        {
            m_rxFrameRecords.append(record);
            while (m_rxFrameRecords.size() > kMaxFrameRows)
            {
                m_rxFrameRecords.removeFirst();
            }
            if (m_saveRxBtn != nullptr)
            {
                m_saveRxBtn->setEnabled(true);
            }
        }

        const bool overwriteRx = !isTx && m_rxOverwriteCheck != nullptr && m_rxOverwriteCheck->isChecked();
        int row = -1;
        if (overwriteRx)
        {
            for (int candidate = 0; candidate < table->rowCount(); ++candidate)
            {
                const QTableWidgetItem *idItem = table->item(candidate, ColID);
                if (idItem != nullptr
                    && idItem->data(CanIdRole).toUInt() == id
                    && idItem->data(ExtendedRole).toBool() == extended)
                {
                    row = candidate;
                    break;
                }
            }
        }

        if (row < 0 && table->rowCount() >= kMaxFrameRows)
        {
            table->removeRow(0);
        }

        if (row < 0)
        {
            row = table->rowCount();
            table->insertRow(row);
        }

        // #
        auto *idxItem = new QTableWidgetItem(QString::number(record.sequence));
        idxItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, ColIndex, idxItem);

        // 时间
        auto *tsItem = new QTableWidgetItem(
            QDateTime::fromMSecsSinceEpoch(record.timestamp).toString(QStringLiteral("HH:mm:ss.zzz")));
        table->setItem(row, ColTimestamp, tsItem);

        // 方向
        auto *dirItem = new QTableWidgetItem(isTx ? tr("TX") : tr("RX"));
        dirItem->setTextAlignment(Qt::AlignCenter);
        QFont badgeFont = dirItem->font();
        badgeFont.setBold(true);
        badgeFont.setPointSize(badgeFont.pointSize() > 0 ? badgeFont.pointSize() + 1 : 11);
        dirItem->setFont(badgeFont);
        if (isTx)
        {
            dirItem->setForeground(QColor(QStringLiteral("#FFFFFF")));
            dirItem->setBackground(QColor(QStringLiteral("#1565C0")));
        }
        else
        {
            dirItem->setForeground(QColor(QStringLiteral("#FFFFFF")));
            dirItem->setBackground(QColor(QStringLiteral("#2E7D32")));
        }
        table->setItem(row, ColDir, dirItem);

        // ID
        const QString idStr = extended
            ? QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')).toUpper()
            : QStringLiteral("0x%1").arg(id, 3, 16, QLatin1Char('0')).toUpper();
        auto *idItem = new QTableWidgetItem(idStr);
        idItem->setFont(QFont(QStringLiteral("Consolas"), isTx ? 10 : 12));
        idItem->setData(CanIdRole, id);
        idItem->setData(ExtendedRole, extended);
        table->setItem(row, ColID, idItem);

        // DLC
        auto *dlcItem = new QTableWidgetItem(QString::number(dlc));
        dlcItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, ColDLC, dlcItem);

        // 数据 (HEX)
        QStringList hexParts;
        for (int i = 0; i < data.size(); ++i)
            hexParts << QStringLiteral("%1").arg(static_cast<quint8>(data.at(i)), 2, 16, QLatin1Char('0')).toUpper();
        auto *dataItem = new QTableWidgetItem(hexParts.join(QStringLiteral(" ")));
        QFont dataFont(QStringLiteral("Consolas"), isTx ? 11 : 13);
        dataFont.setBold(true);
        dataItem->setFont(dataFont);
        table->setItem(row, ColData, dataItem);

        const QColor rowBackground(isTx ? QStringLiteral("#F3F8FF") : QStringLiteral("#F0FFF4"));
        const QColor pulseBackground(isTx ? QStringLiteral("#D7EAFF") : QStringLiteral("#D7FFE3"));
        const QColor idBackground(isTx ? QStringLiteral("#DDEEFF") : QStringLiteral("#DDF8E7"));
        const QColor textColor(QStringLiteral("#111827"));
        for (int col = 0; col < ColCount; ++col)
        {
            QTableWidgetItem *item = table->item(row, col);
            if (item == nullptr || col == ColDir)
            {
                continue;
            }
            item->setBackground(rowBackground);
            item->setForeground(textColor);
            item->setTextAlignment(col == ColData ? (Qt::AlignVCenter | Qt::AlignLeft)
                                                  : Qt::AlignCenter);
        }

        QFont idFont(QStringLiteral("Consolas"), isTx ? 11 : 13);
        idFont.setBold(true);
        idItem->setFont(idFont);
        idItem->setBackground(idBackground);
        table->setRowHeight(row, isTx ? 34 : 42);
        for (int col = 0; col < ColCount; ++col)
        {
            QTableWidgetItem *item = table->item(row, col);
            if (item != nullptr && col != ColDir)
            {
                item->setBackground(pulseBackground);
            }
        }
        const int highlightedRow = row;
        QTimer::singleShot(180, table, [table, highlightedRow, rowBackground, idBackground]() {
            if (highlightedRow < 0 || highlightedRow >= table->rowCount())
            {
                return;
            }
            for (int col = 0; col < ColCount; ++col)
            {
                QTableWidgetItem *item = table->item(highlightedRow, col);
                if (item == nullptr || col == ColDir)
                {
                    continue;
                }
                item->setBackground(col == ColID ? idBackground : rowBackground);
            }
        });

        // 自动滚动到底部
        if (!overwriteRx)
        {
            table->scrollToBottom();
        }
        else
        {
            table->scrollToItem(table->item(row, ColID), QAbstractItemView::PositionAtCenter);
        }

        // 更新统计
        m_statsLabel->setText(tr("共 %1 帧  RX %2 / TX %3").arg(m_frameCount).arg(m_rxCount).arg(m_txCount));
    }

    void CANBusPage::rebuildRxFrameTable()
    {
        if (m_rxFrameTable == nullptr)
        {
            return;
        }

        QList<CanFrameRecord> visibleRecords;
        const bool overwriteRx = m_rxOverwriteCheck != nullptr && m_rxOverwriteCheck->isChecked();
        if (overwriteRx)
        {
            QHash<QString, CanFrameRecord> latestById;
            for (const CanFrameRecord &record : std::as_const(m_rxFrameRecords))
            {
                const QString key = QStringLiteral("%1:%2")
                                        .arg(record.extended ? 1 : 0)
                                        .arg(record.id);
                latestById.insert(key, record);
            }

            visibleRecords = latestById.values();
            std::sort(visibleRecords.begin(), visibleRecords.end(),
                      [](const CanFrameRecord &left, const CanFrameRecord &right) {
                          return left.timestamp < right.timestamp;
                      });
        }
        else
        {
            visibleRecords = m_rxFrameRecords;
        }

        m_rxFrameTable->setRowCount(0);
        for (const CanFrameRecord &record : std::as_const(visibleRecords))
        {
            const int row = m_rxFrameTable->rowCount();
            m_rxFrameTable->insertRow(row);

            auto *idxItem = new QTableWidgetItem(QString::number(record.sequence));
            idxItem->setTextAlignment(Qt::AlignCenter);
            m_rxFrameTable->setItem(row, ColIndex, idxItem);

            auto *tsItem = new QTableWidgetItem(
                QDateTime::fromMSecsSinceEpoch(record.timestamp).toString(QStringLiteral("HH:mm:ss.zzz")));
            m_rxFrameTable->setItem(row, ColTimestamp, tsItem);

            auto *dirItem = new QTableWidgetItem(tr("RX"));
            dirItem->setTextAlignment(Qt::AlignCenter);
            QFont badgeFont = dirItem->font();
            badgeFont.setBold(true);
            badgeFont.setPointSize(badgeFont.pointSize() > 0 ? badgeFont.pointSize() + 1 : 11);
            dirItem->setFont(badgeFont);
            dirItem->setForeground(QColor(QStringLiteral("#FFFFFF")));
            dirItem->setBackground(QColor(QStringLiteral("#2E7D32")));
            m_rxFrameTable->setItem(row, ColDir, dirItem);

            const QString idStr = record.extended
                ? QStringLiteral("0x%1").arg(record.id, 8, 16, QLatin1Char('0')).toUpper()
                : QStringLiteral("0x%1").arg(record.id, 3, 16, QLatin1Char('0')).toUpper();
            auto *idItem = new QTableWidgetItem(idStr);
            idItem->setData(CanIdRole, record.id);
            idItem->setData(ExtendedRole, record.extended);
            m_rxFrameTable->setItem(row, ColID, idItem);

            auto *dlcItem = new QTableWidgetItem(QString::number(record.dlc));
            dlcItem->setTextAlignment(Qt::AlignCenter);
            m_rxFrameTable->setItem(row, ColDLC, dlcItem);

            QStringList hexParts;
            for (int i = 0; i < record.data.size(); ++i)
            {
                hexParts << QStringLiteral("%1").arg(static_cast<quint8>(record.data.at(i)),
                                                      2, 16, QLatin1Char('0')).toUpper();
            }
            auto *dataItem = new QTableWidgetItem(hexParts.join(QStringLiteral(" ")));
            QFont dataFont(QStringLiteral("Consolas"), 13);
            dataFont.setBold(true);
            dataItem->setFont(dataFont);
            m_rxFrameTable->setItem(row, ColData, dataItem);

            const QColor rowBackground(QStringLiteral("#F0FFF4"));
            const QColor idBackground(QStringLiteral("#DDF8E7"));
            const QColor textColor(QStringLiteral("#111827"));
            for (int col = 0; col < ColCount; ++col)
            {
                QTableWidgetItem *item = m_rxFrameTable->item(row, col);
                if (item == nullptr || col == ColDir)
                {
                    continue;
                }
                item->setBackground(col == ColID ? idBackground : rowBackground);
                item->setForeground(textColor);
                item->setTextAlignment(col == ColData ? (Qt::AlignVCenter | Qt::AlignLeft)
                                                      : Qt::AlignCenter);
            }

            QFont idFont(QStringLiteral("Consolas"), 13);
            idFont.setBold(true);
            idItem->setFont(idFont);
            m_rxFrameTable->setRowHeight(row, 42);
        }

        m_rxFrameTable->scrollToBottom();
    }

    void CANBusPage::onPeriodicToggled()
    {
        const bool enabled = m_periodicCheck->isChecked();
        m_periodSpin->setEnabled(enabled);
        if (enabled)
        {
            if (!m_connected)
            {
                m_periodicCheck->setChecked(false);
                QMessageBox::warning(this, tr("提示"), tr("请先打开 CAN"));
                return;
            }
            m_periodicTimer->start(m_periodSpin->value());
            emit statusMessageGenerated(tr("周期发送已启动，每 %1 ms").arg(m_periodSpin->value()));
        }
        else if (m_periodicTimer != nullptr)
        {
            m_periodicTimer->stop();
            emit statusMessageGenerated(tr("周期发送已停止"));
        }
    }

    void CANBusPage::onSaveRxData()
    {
        if (m_rxFrameRecords.isEmpty())
        {
            QMessageBox::information(this, tr("保存 RX"), tr("当前没有可保存的 RX 数据"));
            return;
        }

        const QString defaultName = QStringLiteral("can_rx_%1.csv")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("保存 RX 数据"),
            AppPaths::exportFilePath(defaultName),
            tr("CSV 文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)"));
        if (filePath.isEmpty())
        {
            return;
        }
        if (AppPaths::isDriveCPath(filePath))
        {
            QMessageBox::warning(this, tr("保存失败"),
                                 tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
            return;
        }
        if (!filePath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)
            && !filePath.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
        {
            filePath += QStringLiteral(".csv");
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QMessageBox::warning(this, tr("保存失败"),
                                 tr("无法写入文件：%1").arg(file.errorString()));
            return;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << QStringLiteral("序号,时间,方向,帧类型,ID,DLC,数据\n");
        for (const CanFrameRecord &record : std::as_const(m_rxFrameRecords))
        {
            const QString timestamp = QDateTime::fromMSecsSinceEpoch(record.timestamp)
                                          .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
            const QString idText = record.extended
                ? QStringLiteral("0x%1").arg(record.id, 8, 16, QLatin1Char('0')).toUpper()
                : QStringLiteral("0x%1").arg(record.id, 3, 16, QLatin1Char('0')).toUpper();

            QStringList hexParts;
            for (int i = 0; i < record.data.size(); ++i)
            {
                hexParts << QStringLiteral("%1").arg(static_cast<quint8>(record.data.at(i)),
                                                      2, 16, QLatin1Char('0')).toUpper();
            }

            out << record.sequence << QLatin1Char(',')
                << csvEscaped(timestamp) << QLatin1Char(',')
                << QStringLiteral("RX") << QLatin1Char(',')
                << csvEscaped(record.extended ? tr("扩展帧") : tr("标准帧")) << QLatin1Char(',')
                << csvEscaped(idText) << QLatin1Char(',')
                << record.dlc << QLatin1Char(',')
                << csvEscaped(hexParts.join(QStringLiteral(" "))) << QLatin1Char('\n');
        }

        emit statusMessageGenerated(tr("已保存 RX 数据：%1 帧").arg(m_rxFrameRecords.size()));
    }

    void CANBusPage::onClear()
    {
        if (m_txFrameTable != nullptr)
            m_txFrameTable->setRowCount(0);
        if (m_rxFrameTable != nullptr)
            m_rxFrameTable->setRowCount(0);
        m_frameCount = 0;
        m_rxCount = 0;
        m_txCount = 0;
        m_rxFrameRecords.clear();
        if (m_saveRxBtn != nullptr)
        {
            m_saveRxBtn->setEnabled(false);
        }
        m_statsLabel->setText(tr("共 0 帧"));
        m_lineBuffer.clear();
    }

    QByteArray CANBusPage::buildCANFrame(uint32_t id, int dlc,
                                          const QByteArray &data,
                                          bool extended)
    {
        return SlcanCodec::buildDataFrame(id, dlc, data, extended);
    }

    void CANBusPage::refreshPorts()
    {
        const QString current = currentPortName();
        m_portCombo->clear();

        const QList<QSerialPortInfo> ports = safeAvailablePorts();
        int selectedIndex = 0;
        for (const QSerialPortInfo &port : ports)
        {
            if (!isCanablePort(port))
            {
                continue;
            }

            const int index = m_portCombo->count();
            m_portCombo->addItem(canPortDisplayName(port), port.portName());
            if (!current.isEmpty() && port.portName() == current)
            {
                selectedIndex = index;
            }
        }

        if (m_portCombo->count() == 0)
        {
            m_portCombo->addItem(tr("未检测到 CANable"), QString());
            m_statusLabel->setText(tr("未检测到 CANable"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #FF9800; font-weight: bold;"));
            return;
        }

        m_portCombo->setCurrentIndex(selectedIndex);
        if (!m_connected)
        {
            m_statusLabel->setText(tr("未连接"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #F44336; font-weight: bold;"));
        }
    }

    void CANBusPage::updateDlcFromPayload()
    {
        updatePayloadEditorsFromDlc();
    }

    void CANBusPage::updatePayloadEditorsFromDlc()
    {
        const int dlc = m_dlcSpin != nullptr ? m_dlcSpin->value() : 0;
        for (int index = 0; index < m_dataByteEdits.size(); ++index)
        {
            QLineEdit *byteEdit = m_dataByteEdits.at(index);
            const bool active = index < dlc;
            byteEdit->setEnabled(active);
            if (!active)
            {
                byteEdit->clear();
                continue;
            }

            QString text = byteEdit->text().trimmed().toUpper();
            if (text.isEmpty())
            {
                text = QStringLiteral("00");
            }
            byteEdit->setText(text);
        }
    }

    void CANBusPage::publishFrame(uint32_t id, int dlc, const QByteArray &data,
                                  bool extended, bool isTx)
    {
        if (m_dataBus == nullptr)
        {
            return;
        }

        DataPacket packet;
        packet.timestamp = QDateTime::currentMSecsSinceEpoch();
        packet.channel = QStringLiteral("transport.can.slcan");
        packet.rawPayload = data;
        packet.metadata.insert(QStringLiteral("can_id"), static_cast<qulonglong>(id));
        packet.metadata.insert(QStringLiteral("dlc"), dlc);
        packet.metadata.insert(QStringLiteral("extended"), extended);
        packet.metadata.insert(QStringLiteral("direction"), isTx ? QStringLiteral("tx") : QStringLiteral("rx"));
        packet.metadata.insert(QStringLiteral("bitrate"), selectedBitrate());
        m_dataBus->publish(packet.channel, packet);
    }

    int CANBusPage::selectedBitrate() const
    {
        return m_baudRateCombo->currentData().toInt();
    }

    QString CANBusPage::currentPortName() const
    {
        const QString dataPortName = m_portCombo->currentData().toString().trimmed();
        if (!dataPortName.isEmpty())
        {
            return dataPortName;
        }

        const QString text = m_portCombo->currentText().trimmed();
        static const QRegularExpression s_portName(QStringLiteral("\\b(COM\\d+|/dev/tty\\S+|/dev/cu\\.\\S+)\\b"),
                                                   QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = s_portName.match(text);
        if (match.hasMatch())
        {
            return match.captured(1);
        }
        return {};
    }

    void CANBusPage::setConnectedUi(bool connected, const QString &statusText)
    {
        m_connected = connected;
        m_sendBtn->setEnabled(connected);
        m_portCombo->setEnabled(!connected);
        m_refreshPortsBtn->setEnabled(!connected);
        m_baudRateCombo->setEnabled(!connected);
        m_connectBtn->setText(connected ? tr("关闭 CAN") : tr("打开 CAN"));
        m_statusLabel->setText(statusText.isEmpty() ? (connected ? tr("CAN 已打开") : tr("未连接")) : statusText);
        m_statusLabel->setStyleSheet(connected
            ? QStringLiteral("color: #4CAF50; font-weight: bold;")
            : QStringLiteral("color: #F44336; font-weight: bold;"));
    }

} // namespace est
