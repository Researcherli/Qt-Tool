#include "transport/SerialTransport.h"

#include <QDateTime>
#include <QVariant>

#include <QSerialPort>

namespace est
{

    namespace
    {

        QSerialPort::DataBits dataBitsFromConfig(const QVariant &value)
        {
            switch (value.toInt())
            {
            case 5:
                return QSerialPort::Data5;
            case 6:
                return QSerialPort::Data6;
            case 7:
                return QSerialPort::Data7;
            case 8:
            default:
                return QSerialPort::Data8;
            }
        }

        QSerialPort::Parity parityFromConfig(const QVariant &value)
        {
            const QString text = value.toString().toLower();
            if (text == QStringLiteral("odd"))
            {
                return QSerialPort::OddParity;
            }
            if (text == QStringLiteral("even"))
            {
                return QSerialPort::EvenParity;
            }
            return QSerialPort::NoParity;
        }

        QSerialPort::StopBits stopBitsFromConfig(const QVariant &value)
        {
            const QString text = value.toString();
            if (text == QStringLiteral("1.5"))
            {
                return QSerialPort::OneAndHalfStop;
            }
            if (text == QStringLiteral("2"))
            {
                return QSerialPort::TwoStop;
            }
            return QSerialPort::OneStop;
        }

    } // namespace

    SerialTransport::SerialTransport(QObject *parent)
        : ITransport(parent), m_serial(new QSerialPort(this))
    {
        connect(m_serial, &QSerialPort::readyRead,
                this, &SerialTransport::onReadyRead);
        connect(m_serial, &QSerialPort::errorOccurred,
                this, &SerialTransport::onErrorOccurred);
    }

    SerialTransport::~SerialTransport() = default;

    QString SerialTransport::transportType() const
    {
        return QStringLiteral("serial");
    }

    QString SerialTransport::name() const
    {
        return m_portName;
    }

    ITransport::State SerialTransport::state() const
    {
        return m_state;
    }

    bool SerialTransport::open(const QVariantMap &config)
    {
        const QString portName = config.value(QStringLiteral("portName")).toString();
        if (portName.isEmpty())
        {
            emit errorOccurred(tr("串口名称为空"));
            setState(State::Error);
            return false;
        }

        if (m_serial->isOpen())
        {
            close();
        }

        setState(State::Connecting);
        m_portName = portName;

        m_serial->setPortName(portName);
        m_serial->setBaudRate(config.value(QStringLiteral("baudRate"), 115200).toInt());
        m_serial->setDataBits(dataBitsFromConfig(config.value(QStringLiteral("dataBits"), 8)));
        m_serial->setParity(parityFromConfig(config.value(QStringLiteral("parity"), QStringLiteral("none"))));
        m_serial->setStopBits(stopBitsFromConfig(config.value(QStringLiteral("stopBits"), QStringLiteral("1"))));
        m_serial->setFlowControl(QSerialPort::NoFlowControl);

        if (!m_serial->open(QIODevice::ReadWrite))
        {
            emit errorOccurred(tr("打开串口失败：%1").arg(m_serial->errorString()));
            setState(State::Error);
            setState(State::Disconnected);
            return false;
        }

        // 预计算通道名和元数据，提高高频接收的性能
        m_channelName = QStringLiteral("transport.serial.%1").arg(m_portName);
        m_rxMetadata.clear();
        m_rxMetadata.insert(QStringLiteral("direction"), QStringLiteral("rx"));
        m_rxMetadata.insert(QStringLiteral("portName"), m_portName);
        m_rxMetadata.insert(QStringLiteral("transportType"), transportType());

        setState(State::Connected);
        return true;
    }

    void SerialTransport::close()
    {
        if (m_serial->isOpen())
        {
            m_serial->close();
        }

        setState(State::Disconnected);
    }

    bool SerialTransport::send(const DataPacket &packet)
    {
        if (!m_serial->isOpen() || m_state != State::Connected)
        {
            emit errorOccurred(tr("串口尚未连接"));
            return false;
        }

        const qint64 written = m_serial->write(packet.rawPayload);
        if (written < 0)
        {
            emit errorOccurred(tr("写入串口失败：%1").arg(m_serial->errorString()));
            setState(State::Error);
            return false;
        }

        return written == packet.rawPayload.size();
    }

    void SerialTransport::onReadyRead()
    {
        const QByteArray payload = m_serial->readAll();
        if (payload.isEmpty())
        {
            return;
        }

        DataPacket packet;
        packet.timestamp = QDateTime::currentMSecsSinceEpoch();
        packet.channel = m_channelName;
        packet.rawPayload = payload;
        packet.metadata = m_rxMetadata; // 共享 QVariantMap（隐式共享机制，性能极高）
        emit dataReceived(packet);
    }

    void SerialTransport::onErrorOccurred()
    {
        const QSerialPort::SerialPortError error = m_serial->error();
        if (error == QSerialPort::NoError || error == QSerialPort::NotOpenError)
        {
            return;
        }

        emit errorOccurred(tr("%1").arg(m_serial->errorString()));
        setState(State::Error);

        if (error == QSerialPort::ResourceError)
        {
            if (m_serial->isOpen())
            {
                m_serial->close();
            }
            setState(State::Disconnected);
        }
    }

    void SerialTransport::setState(State newState)
    {
        if (m_state == newState)
        {
            return;
        }

        m_state = newState;
        emit stateChanged(m_state);
    }

} // namespace est
