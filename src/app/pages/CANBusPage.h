#ifndef EST_CANBUSPAGE_H
#define EST_CANBUSPAGE_H

#include <QWidget>

#include <QByteArray>
#include <QList>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "databus/SubscriptionHandle.h"
#include "transport/ITransport.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QTimer;

namespace est
{

    class DataBus;
    class ICore;
    class ITransport;
    struct DataPacket;

    /**
     * CAN 总线页面 — 通过 CANable (slcan 协议) 收发 CAN 帧。
     *
     * 布局：上 → 连接栏 + 发送面板，下 → 接收帧表格
     */
    class CANBusPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit CANBusPage(ICore *core, QWidget *parent = nullptr);
        ~CANBusPage() override;

    signals:
        void statusMessageGenerated(const QString &text);

    private slots:
        void onConnectToggle();
        void onSend();
        void onPeriodicToggled();
        void onClear();
        void onSaveRxData();
        void refreshPorts();
        void updateDlcFromPayload();
        void updatePayloadEditorsFromDlc();
        void onTransportData(const DataPacket &packet);
        void onTransportStateChanged(ITransport::State state);
        void onTransportError(const QString &message);

    private:
        void setupUi();
        void setupConnectionBar(QVBoxLayout *root);
        void setupSendPanel(QVBoxLayout *root);
        void setupReceiveTable(QVBoxLayout *root);
        QTableWidget *createFrameTable(QWidget *parent);

        void openCANable();
        void closeCANable();
        void sendSLCAN(const QByteArray &cmd);
        void parseIncomingData(const QByteArray &data);
        void sendCurrentFrame(bool fromPeriodic);
        QByteArray buildCANFrame(uint32_t id, int dlc, const QByteArray &data, bool extended);
        void addFrameToTable(uint32_t id, int dlc, const QByteArray &data,
                              bool extended, bool isTx);
        void publishFrame(uint32_t id, int dlc, const QByteArray &data,
                          bool extended, bool isTx);
        void rebuildRxFrameTable();
        int selectedBitrate() const;
        QString currentPortName() const;
        void setConnectedUi(bool connected, const QString &statusText = QString());

        struct CanFrameRecord
        {
            int sequence = 0;
            qint64 timestamp = 0;
            uint32_t id = 0;
            int dlc = 0;
            QByteArray data;
            bool extended = false;
            bool isTx = false;
        };

        // DataBus + Transport
        ICore *m_core = nullptr;
        DataBus *m_dataBus = nullptr;
        ITransport *m_transport = nullptr;
        SubscriptionHandle m_subscription;

        // 连接栏
        QComboBox *m_portCombo = nullptr;
        QPushButton *m_refreshPortsBtn = nullptr;
        QComboBox *m_baudRateCombo = nullptr;
        QPushButton *m_connectBtn = nullptr;
        QLabel *m_statusLabel = nullptr;

        // 发送面板
        QLineEdit *m_idEdit = nullptr;
        QComboBox *m_idTypeCombo = nullptr;
        QSpinBox *m_dlcSpin = nullptr;
        QLineEdit *m_dataEdit = nullptr;
        QList<QLineEdit *> m_dataByteEdits;
        QCheckBox *m_periodicCheck = nullptr;
        QSpinBox *m_periodSpin = nullptr;
        QTimer *m_periodicTimer = nullptr;
        QPushButton *m_sendBtn = nullptr;

        // 接收表格
        QTableWidget *m_txFrameTable = nullptr;
        QTableWidget *m_rxFrameTable = nullptr;
        QPushButton *m_saveRxBtn = nullptr;
        QPushButton *m_clearBtn = nullptr;
        QLabel *m_statsLabel = nullptr;
        QCheckBox *m_rxOverwriteCheck = nullptr;

        // 状态
        int m_frameCount = 0;
        int m_rxCount = 0;
        int m_txCount = 0;
        QList<CanFrameRecord> m_rxFrameRecords;
        bool m_connected = false;
        QString m_lineBuffer;      // slcan 行缓冲区
        bool m_closing = false;
        QString m_lastTransportError;
    };

} // namespace est

#endif // EST_CANBUSPAGE_H
