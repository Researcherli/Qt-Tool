#include "services/DataLogger.h"

#include "services/AppPaths.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTime>
#include <limits>

namespace est
{

    // =========================================================================
    // DataLogger
    // =========================================================================

    DataLogger::DataLogger(QObject *parent)
        : QObject(parent)
    {
    }

    DataLogger::~DataLogger()
    {
        stopRecording();
    }

    bool DataLogger::startRecording(const QString &filePath)
    {
        if (m_recording)
            stopRecording();

        QString path = filePath;
        if (path.isEmpty())
        {
            const QString dir = defaultLogDirectory();
            const QString timestamp = QDateTime::currentDateTime()
                                          .toString(QStringLiteral("yyyyMMdd_HHmmss"));
            path = QDir(dir).filePath(QStringLiteral("est_%1.estlog").arg(timestamp));
        }

        const QFileInfo info(path);
        if (!info.dir().mkpath(QStringLiteral(".")))
        {
            emit recordingError(tr("无法创建日志目录：%1").arg(info.absolutePath()));
            return false;
        }

        m_file = new QFile(path, this);
        if (!m_file->open(QIODevice::WriteOnly | QIODevice::Text))
        {
            emit recordingError(tr("无法写入日志文件：%1").arg(path));
            delete m_file;
            m_file = nullptr;
            return false;
        }

        m_stream = new QTextStream(m_file);
        m_filePath = path;
        m_recording = true;
        m_frameCount = 0;
        m_bytesWritten = 0;
        m_elapsed.start();

        emit recordingStarted(path);
        return true;
    }

    void DataLogger::record(const DataPacket &packet)
    {
        if (!m_recording || !m_stream)
            return;

        const QByteArray line = serialize(packet);
        *m_stream << QString::fromUtf8(line) << QStringLiteral("\n");

        m_frameCount++;
        m_bytesWritten += line.size() + 1;

        // Flush every frame (for crash resilience), rate-limited to every 500ms
        m_stream->flush();
        if (m_frameCount % 100 == 0)
        {
            emit recordingProgress(m_frameCount, m_bytesWritten, m_elapsed.elapsed());
        }
    }

    void DataLogger::stopRecording()
    {
        if (!m_recording)
            return;

        m_recording = false;

        if (m_stream)
        {
            m_stream->flush();
            delete m_stream;
            m_stream = nullptr;
        }
        if (m_file)
        {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }

        emit recordingStopped(m_filePath, m_frameCount, m_bytesWritten);

        m_frameCount = 0;
        m_bytesWritten = 0;
        m_filePath.clear();
    }

    QTime DataLogger::elapsedTime() const
    {
        if (!m_recording)
            return QTime(0, 0, 0);
        return QTime::fromMSecsSinceStartOfDay(
            static_cast<int>(m_elapsed.elapsed()));
    }

    QString DataLogger::defaultLogDirectory()
    {
        return AppPaths::logsDir();
    }

    QByteArray DataLogger::serialize(const DataPacket &packet)
    {
        QJsonObject obj;
        obj[QStringLiteral("v")] = 1;
        obj[QStringLiteral("ts")] = packet.timestamp;
        obj[QStringLiteral("ch")] = packet.channel;
        obj[QStringLiteral("payload")] = QString::fromLatin1(
            packet.rawPayload.toBase64());

        if (!packet.metadata.isEmpty())
        {
            QJsonObject meta;
            for (auto it = packet.metadata.cbegin(); it != packet.metadata.cend(); ++it)
                meta[it.key()] = QJsonValue::fromVariant(it.value());
            obj[QStringLiteral("meta")] = meta;
        }

        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    DataPacket DataLogger::deserialize(const QByteArray &line)
    {
        DataPacket packet;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return packet;

        const QJsonObject obj = doc.object();
        packet.timestamp = obj.value(QStringLiteral("ts")).toVariant().toLongLong();
        packet.channel = obj.value(QStringLiteral("ch")).toString();

        const QString hexPayload = obj.value(QStringLiteral("payload")).toString();
        packet.rawPayload = QByteArray::fromBase64(hexPayload.toLatin1());

        if (obj.contains(QStringLiteral("meta")))
        {
            const QJsonObject meta = obj.value(QStringLiteral("meta")).toObject();
            for (auto it = meta.constBegin(); it != meta.constEnd(); ++it)
                packet.metadata.insert(it.key(), it.value().toVariant());
        }

        return packet;
    }

    // =========================================================================
    // DataPlayer
    // =========================================================================

    DataPlayer::DataPlayer(QObject *parent)
        : QObject(parent)
        , m_timer(new QTimer(this))
    {
        m_timer->setTimerType(Qt::PreciseTimer);
        connect(m_timer, &QTimer::timeout, this, &DataPlayer::onTimerTick);
    }

    DataPlayer::~DataPlayer()
    {
        stop();
    }

    bool DataPlayer::loadFile(const QString &path)
    {
        stop();
        reset();

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            emit playbackError(tr("无法打开回放文件：%1").arg(path));
            return false;
        }

        // Check file size before loading into memory
        const qint64 fileBytes = file.size();
        if (fileBytes > 100 * 1024 * 1024) {  // 100MB
            emit playbackError(tr("文件过大（%1 MB），无法加载到内存进行回放。").arg(fileBytes / (1024 * 1024)));
            file.close();
            return false;
        }

        QVector<DataPacket> packets;
        qint64 minTs = std::numeric_limits<qint64>::max();

        while (!file.atEnd())
        {
            const QByteArray line = file.readLine().trimmed();
            if (line.isEmpty())
                continue;

            DataPacket packet = DataLogger::deserialize(line);
            if (packet.channel.isEmpty())
                continue;

            if (packet.timestamp < minTs)
                minTs = packet.timestamp;

            packets.append(packet);
        }

        file.close();

        if (packets.isEmpty())
        {
            emit playbackError(tr("回放文件没有可用帧：%1").arg(path));
            return false;
        }

        m_packets = packets;
        m_baseTimestamp = (minTs == std::numeric_limits<qint64>::max()) ? 0 : minTs;
        m_filePath = path;
        m_currentIndex = 0;

        emit fileLoaded(path, m_packets.size());
        return true;
    }

    void DataPlayer::play()
    {
        if (m_packets.isEmpty())
            return;

        if (m_paused)
        {
            // 从暂停恢复：累计已播放时间
            m_paused = false;
            m_playbackStartMs = QDateTime::currentMSecsSinceEpoch();
            scheduleNextTick();
            emit playbackStarted();
            return;
        }

        if (m_playing)
            return;

        m_playing = true;
        m_currentIndex = 0;
        m_lastSentIndex = -1;
        m_accumulatedElapsedMs = 0;
        m_playbackStartMs = QDateTime::currentMSecsSinceEpoch();
        scheduleNextTick();

        emit playbackStarted();
    }

    void DataPlayer::pause()
    {
        if (!m_playing)
            return;

        m_paused = true;
        m_timer->stop();
        // 累计已播放的"虚拟时间"（含倍速）
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_playbackStartMs;
        m_accumulatedElapsedMs += static_cast<qint64>(elapsed * m_speed);
        emit playbackPaused();
    }

    void DataPlayer::stop()
    {
        m_timer->stop();
        m_playing = false;
        m_paused = false;
        m_currentIndex = 0;
        m_lastSentIndex = -1;
        m_accumulatedElapsedMs = 0;

        if (!m_packets.isEmpty())
            emit playbackStopped();
    }

    void DataPlayer::setSpeed(double speed)
    {
        // 调整前先累计当前时段的已播放时间
        const bool wasPlaying = m_playing && !m_paused;
        if (wasPlaying)
        {
            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_playbackStartMs;
            m_accumulatedElapsedMs += static_cast<qint64>(elapsed * m_speed);
        }

        m_speed = qBound(0.1, speed, 10.0);

        if (wasPlaying)
        {
            m_playbackStartMs = QDateTime::currentMSecsSinceEpoch();
        }
    }

    void DataPlayer::seekTo(double progress)
    {
        if (m_packets.isEmpty())
            return;

        const double clampedProgress = qBound(0.0, progress, 1.0);
        const int index = qBound(0, static_cast<int>(clampedProgress * m_packets.size()),
                                 m_packets.size() - 1);
        m_currentIndex = index;
        m_lastSentIndex = index - 1;

        // 重新计算累计时间：定位到跳转目标帧的偏移
        m_accumulatedElapsedMs = packetOffsetMs(index);
        if (m_playing && !m_paused)
        {
            m_playbackStartMs = QDateTime::currentMSecsSinceEpoch();
        }

        emit playbackProgress(m_currentIndex, m_packets.size(), this->progress());
    }

    double DataPlayer::progress() const
    {
        if (m_packets.isEmpty())
            return 0.0;
        return static_cast<double>(m_currentIndex) / m_packets.size();
    }

    QString DataPlayer::fileName() const
    {
        return QFileInfo(m_filePath).fileName();
    }

    void DataPlayer::onTimerTick()
    {
        if (!m_playing || m_paused || m_packets.isEmpty())
            return;

        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_playbackStartMs;
        const qint64 targetTime = m_accumulatedElapsedMs + static_cast<qint64>(elapsed * m_speed);

        while (m_currentIndex < m_packets.size())
        {
            const auto &pkt = m_packets[m_currentIndex];
            const qint64 pktTime = packetOffsetMs(m_currentIndex);

            if (pktTime <= targetTime)
            {
                DataPacket replayPacket = pkt;
                replayPacket.metadata.insert(QStringLiteral("est.replayed"), true);
                emit packetReady(replayPacket);
                m_lastSentIndex = m_currentIndex;
                m_currentIndex++;

                emit playbackProgress(m_currentIndex, m_packets.size(), progress());
            }
            else
            {
                break;
            }
        }

        if (m_currentIndex >= m_packets.size())
        {
            // 播放完毕
            m_timer->stop();
            m_playing = false;
            emit playbackFinished();
        }
    }

    void DataPlayer::scheduleNextTick()
    {
        // Dynamic timer interval based on packet spacing
        int interval = 5;
        if (!m_packets.isEmpty()) {
            // Calculate average packet interval in the next 100 frames
            int maxIndex = qMin(m_currentIndex + 100, m_packets.size());
            if (maxIndex > m_currentIndex + 1) {
                qint64 totalInterval = packetOffsetMs(maxIndex - 1) - packetOffsetMs(m_currentIndex);
                int avgInterval = static_cast<int>(totalInterval / (maxIndex - m_currentIndex));
                if (avgInterval > 0) {
                    // Use half the average interval for smooth playback, capped at 50ms
                    interval = qBound(5, avgInterval / 2, 50);
                }
            }
        }
        m_timer->start(interval);
    }

    void DataPlayer::reset()
    {
        m_packets.clear();
        m_baseTimestamp = 0;
        m_currentIndex = 0;
        m_playing = false;
        m_paused = false;
        m_speed = 1.0;
        m_lastSentIndex = -1;
        m_accumulatedElapsedMs = 0;
        m_filePath.clear();
    }

    qint64 DataPlayer::packetOffsetMs(int index) const
    {
        if (m_packets.isEmpty())
            return 0;

        const int boundedIndex = qBound(0, index, m_packets.size() - 1);
        return qMax<qint64>(0, m_packets[boundedIndex].timestamp - m_baseTimestamp);
    }

} // namespace est
