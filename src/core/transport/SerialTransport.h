#ifndef EST_SERIALTRANSPORT_H
#define EST_SERIALTRANSPORT_H

#include "transport/ITransport.h"

class QSerialPort;

namespace est
{

    class SerialTransport : public ITransport
    {
        Q_OBJECT

    public:
        explicit SerialTransport(QObject *parent = nullptr);
        ~SerialTransport() override;

        QString transportType() const override;
        QString name() const override;
        State state() const override;

        bool open(const QVariantMap &config) override;
        void close() override;
        bool send(const DataPacket &packet) override;

    private slots:
        void onReadyRead();
        void onErrorOccurred();

    private:
        void setState(State newState);

        QSerialPort *m_serial = nullptr;
        State m_state = State::Disconnected;
        QString m_portName;
        
        // 预缓存数据以优化高频接收性能
        QString m_channelName;
        QVariantMap m_rxMetadata;
    };

} // namespace est

#endif // EST_SERIALTRANSPORT_H
