#ifndef EST_DATALOGGER_H
#define EST_DATALOGGER_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include "databus/DataPacket.h"

class QFile;
class QTextStream;

namespace est
{

    // =========================================================================
    // DataLogger — 录制引擎
    // =========================================================================

    /**
     * 数据录制器 — 将 DataBus 上的数据包录制到 .estlog 文件。
     *
     * 格式：每行一个 JSON 对象（JSONL）
     * {"ts":<毫秒>,"ch":"<通道>","payload":"<base64>"}
     */
    class DataLogger : public QObject
    {
        Q_OBJECT

    public:
        explicit DataLogger(QObject *parent = nullptr);
        ~DataLogger() override;

        /// 开始录制到指定文件（自动生成文件名）
        bool startRecording(const QString &filePath = QString());

        /// 录制一个数据包
        void record(const DataPacket &packet);

        /// 停止录制
        void stopRecording();

        /// 状态查询
        bool isRecording() const { return m_recording; }
        QString currentFilePath() const { return m_filePath; }
        int recordedFrames() const { return m_frameCount; }
        qint64 recordedBytes() const { return m_bytesWritten; }
        QTime elapsedTime() const;

        static QString defaultLogDirectory();

        /// 序列化/反序列化工具
        static QByteArray serialize(const DataPacket &packet);
        static DataPacket deserialize(const QByteArray &line);

    signals:
        void recordingStarted(const QString &filePath);
        void recordingStopped(const QString &filePath, int frames, qint64 bytes);
        void recordingProgress(int frames, qint64 bytes, qint64 elapsedMs);
        void recordingError(const QString &message);

    private:
        QFile *m_file = nullptr;
        QTextStream *m_stream = nullptr;
        QString m_filePath;
        bool m_recording = false;
        int m_frameCount = 0;
        qint64 m_bytesWritten = 0;
        QElapsedTimer m_elapsed;
    };

    // =========================================================================
    // DataPlayer — 回放引擎
    // =========================================================================

    /**
     * 数据回放器 — 加载 .estlog 文件，按原始时间间隔模拟发布到 DataBus。
     *
     * 支持变速播放 (0.1x ~ 10x)、暂停、seek、进度查询。
     */
    class DataPlayer : public QObject
    {
        Q_OBJECT

    public:
        explicit DataPlayer(QObject *parent = nullptr);
        ~DataPlayer() override;

        /// 加载文件（加载后停在第一帧）
        bool loadFile(const QString &path);

        /// 播放控制
        void play();
        void pause();
        void stop();
        void setSpeed(double speed);
        void seekTo(double progress); ///< 0.0 ~ 1.0

        /// 状态查询
        bool isLoaded() const { return !m_packets.isEmpty(); }
        bool isPlaying() const { return m_playing; }
        bool isPaused() const { return m_paused; }
        double speed() const { return m_speed; }
        int totalFrames() const { return m_packets.size(); }
        int currentFrame() const { return m_currentIndex; }
        double progress() const; ///< 0.0 ~ 1.0
        QString filePath() const { return m_filePath; }
        QString fileName() const;

    signals:
        void fileLoaded(const QString &path, int frames);
        void playbackStarted();
        void playbackPaused();
        void playbackStopped();
        void playbackFinished();
        void playbackProgress(int currentFrame, int totalFrames, double progress);
        void packetReady(const DataPacket &packet);
        void playbackError(const QString &message);

    private slots:
        void onTimerTick();

    private:
        void reset();
        void updatePlaybackAnchor();
        qint64 packetOffsetMs(int index) const;
        void scheduleNextTick();

        QVector<DataPacket> m_packets;
        qint64 m_baseTimestamp = 0;         ///< 第一帧的时间戳（基准）
        int m_currentIndex = 0;
        bool m_playing = false;
        bool m_paused = false;
        double m_speed = 1.0;
        qint64 m_playbackStartMs = 0;       ///< 本次播放开始（或恢复）的系统时间
        qint64 m_accumulatedElapsedMs = 0;   ///< 暂停前的累计播放时间（毫秒）
        int m_lastSentIndex = -1;

        QTimer *m_timer = nullptr;
        QString m_filePath;
    };

} // namespace est

#endif // EST_DATALOGGER_H
