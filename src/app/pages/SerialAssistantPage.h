#ifndef EST_SERIALASSISTANTPAGE_H
#define EST_SERIALASSISTANTPAGE_H

#include <QWidget>
#include <memory>

class QTimer;

namespace est
{

    class ICore;
    class DataBus;
    struct DataPacket;
    struct QuickCommandDefinition;
    class ITransport;
    class QuickCommandPanel;
    class RecentRecordManager;
    class SerialConfigBar;
    class SerialReceiveView;
    class SerialSendPanel;
    class TransportRegistry;

    class SerialAssistantPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit SerialAssistantPage(ICore *core,
                                     QWidget *parent = nullptr);
        ~SerialAssistantPage() override;

    signals:
        void serialStatusChanged(const QString &text, bool connected);
        void systemLogGenerated(const QString &text);
        void transferStatsChanged(qint64 txBytes, qint64 rxBytes);
        void recentRecordsChanged();

    private:
        void refreshPorts();
        bool ensureTransport();
        void openCurrentPort();
        void closeCurrentPort();
        void handlePacketReceived(const DataPacket &packet);
        void updateAutoSendState();
        void sendCurrentPayload();
        void clearTransferData();
        void sendCommand(const est::QuickCommandDefinition &command);
        void saveDraftAsQuickCommand();
        void loadSettings();
        void saveSettings() const;

        DataBus *m_dataBus = nullptr;
        TransportRegistry *m_transportRegistry = nullptr;
        ITransport *m_transport = nullptr;
        QString m_lastPortName;
        qint64 m_txBytes = 0;
        qint64 m_rxBytes = 0;
        SerialConfigBar *m_configBar = nullptr;
        SerialReceiveView *m_receiveView = nullptr;
        QuickCommandPanel *m_quickCommandPanel = nullptr;
        SerialSendPanel *m_sendPanel = nullptr;
        RecentRecordManager *m_recentRecordManager = nullptr;
        QTimer *m_autoSendTimer = nullptr;
    };

} // namespace est

#endif // EST_SERIALASSISTANTPAGE_H
