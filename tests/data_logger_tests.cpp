#include "databus/DataBus.h"
#include "services/DataLogger.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace est;

class DataLoggerTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<est::DataPacket>("est::DataPacket");
    }

    void serializeRoundTripsPacketMetadata()
    {
        DataPacket packet;
        packet.timestamp = 1234;
        packet.channel = QStringLiteral("transport.serial.COM1");
        packet.rawPayload = QByteArray::fromHex("0102ff");
        packet.metadata.insert(QStringLiteral("direction"), QStringLiteral("rx"));
        packet.metadata.insert(QStringLiteral("baudRate"), 115200);

        const DataPacket parsed = DataLogger::deserialize(DataLogger::serialize(packet));

        QCOMPARE(parsed.timestamp, packet.timestamp);
        QCOMPARE(parsed.channel, packet.channel);
        QCOMPARE(parsed.rawPayload, packet.rawPayload);
        QCOMPARE(parsed.metadata.value(QStringLiteral("direction")).toString(), QStringLiteral("rx"));
        QCOMPARE(parsed.metadata.value(QStringLiteral("baudRate")).toInt(), 115200);
    }

    void dataBusPublishesObserverSignalWithoutSubscribers()
    {
        DataBus bus;
        QSignalSpy spy(&bus, &DataBus::dataPublished);

        DataPacket packet;
        packet.channel = QStringLiteral("transport.serial.COM1");
        packet.rawPayload = QByteArrayLiteral("abc");
        bus.publish(packet.channel, packet);

        QCOMPARE(spy.size(), 1);
    }

    void recorderWritesDataBusPacketsWithoutSubscribers()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("capture.estlog"));

        DataBus bus;
        DataLogger logger;
        connect(&bus, &DataBus::dataPublished, &logger,
                [&logger](const QString &, const DataPacket &packet) {
                    logger.record(packet);
                });

        QVERIFY(logger.startRecording(path));

        DataPacket packet;
        packet.timestamp = 42;
        packet.channel = QStringLiteral("transport.serial.COM1");
        packet.rawPayload = QByteArray::fromHex("aabb");
        bus.publish(packet.channel, packet);
        logger.stopRecording();

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray line = file.readLine().trimmed();
        QVERIFY(!line.isEmpty());

        const DataPacket parsed = DataLogger::deserialize(line);
        QCOMPARE(parsed.channel, packet.channel);
        QCOMPARE(parsed.rawPayload, packet.rawPayload);
    }

    void playerLoadsZeroTimestampLogsAndMarksReplayedPackets()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("zero.estlog"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

        DataPacket first;
        first.timestamp = 0;
        first.channel = QStringLiteral("transport.serial.COM1");
        first.rawPayload = QByteArrayLiteral("a");
        file.write(DataLogger::serialize(first));
        file.write("\n");

        DataPacket second = first;
        second.rawPayload = QByteArrayLiteral("b");
        file.write(DataLogger::serialize(second));
        file.write("\n");
        file.close();

        DataPlayer player;
        QSignalSpy packetSpy(&player, &DataPlayer::packetReady);

        QVERIFY(player.loadFile(path));
        QCOMPARE(player.totalFrames(), 2);

        player.play();
        QTRY_COMPARE(packetSpy.size(), 2);

        const auto args = packetSpy.first();
        const DataPacket replayed = qvariant_cast<DataPacket>(args.at(0));
        QVERIFY(replayed.metadata.value(QStringLiteral("est.replayed")).toBool());
    }

    void playerClampsSeekProgress()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("seek.estlog"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        for (int i = 0; i < 4; ++i)
        {
            DataPacket packet;
            packet.timestamp = i * 10;
            packet.channel = QStringLiteral("transport.serial.COM1");
            packet.rawPayload = QByteArray(1, static_cast<char>('0' + i));
            file.write(DataLogger::serialize(packet));
            file.write("\n");
        }
        file.close();

        DataPlayer player;
        QVERIFY(player.loadFile(path));

        player.seekTo(2.0);
        QCOMPARE(player.currentFrame(), 3);
        QVERIFY(player.progress() <= 1.0);

        player.seekTo(-1.0);
        QCOMPARE(player.currentFrame(), 0);
        QCOMPARE(player.progress(), 0.0);
    }
};

QTEST_MAIN(DataLoggerTests)

#include "data_logger_tests.moc"
