#include <QTest>
#include <QSignalSpy>
#include "transport/SerialTransport.h"
#include "transport/ITransport.h"

using namespace est;

class TestSerialTransport : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // No real ports needed - tested with error paths
    }

    void initialState() {
        SerialTransport transport;
        QCOMPARE(transport.state(), ITransport::State::Disconnected);
        QCOMPARE(transport.transportType(), QStringLiteral("serial"));
        QVERIFY(transport.name().isEmpty());
    }

    void openWithEmptyPortName() {
        SerialTransport transport;
        QSignalSpy errorSpy(&transport, &ITransport::errorOccurred);
        QSignalSpy stateSpy(&transport, &ITransport::stateChanged);

        QVariantMap config;
        config.insert("portName", QString());
        bool result = transport.open(config);

        QVERIFY(!result);  // Should fail with empty port name
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(transport.state(), ITransport::State::Disconnected);
    }

    void openNonexistentPort() {
        SerialTransport transport;
        QSignalSpy errorSpy(&transport, &ITransport::errorOccurred);

        QVariantMap config;
        config.insert("portName", QStringLiteral("COM_DOES_NOT_EXIST_99999"));
        config.insert("baudRate", 115200);
        bool result = transport.open(config);

        QVERIFY(!result);
        QVERIFY(errorSpy.count() > 0);
    }

    void closeWhenNotOpen() {
        SerialTransport transport;
        // Closing when not open should not crash
        transport.close();
        QCOMPARE(transport.state(), ITransport::State::Disconnected);
    }

    void sendWhenNotConnected() {
        SerialTransport transport;
        QSignalSpy errorSpy(&transport, &ITransport::errorOccurred);

        DataPacket packet;
        packet.channel = QStringLiteral("test");
        packet.rawPayload = QByteArrayLiteral("test");
        bool result = transport.send(packet);

        QVERIFY(!result);
        QCOMPARE(errorSpy.count(), 1);
    }
};

QTEST_MAIN(TestSerialTransport)
#include "serial_transport_tests.moc"
