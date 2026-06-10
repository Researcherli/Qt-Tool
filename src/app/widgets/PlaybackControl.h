#ifndef EST_PLAYBACKCONTROL_H
#define EST_PLAYBACKCONTROL_H

#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class QToolButton;

namespace est
{

    class DataPlayer;

    /**
     * 回放控制浮动栏 — 播放/暂停/停止/进度/变速。
     */
    class PlaybackControl : public QWidget
    {
        Q_OBJECT

    public:
        explicit PlaybackControl(QWidget *parent = nullptr);

        void bindPlayer(DataPlayer *player);

    signals:
        void loadFileRequested();
        void closeRequested();

    private slots:
        void onPlayPause();
        void onStop();
        void onSpeedChanged();
        void onSliderPressed();
        void onSliderReleased();
        void onProgressUpdated(int current, int total, double progress);
        void onPlaybackFinished();

    private:
        void setupUi();
        void updateState();

        DataPlayer *m_player = nullptr;

        QToolButton *m_playBtn = nullptr;
        QToolButton *m_stopBtn = nullptr;
        QSlider *m_progressSlider = nullptr;
        QPushButton *m_speedBtn = nullptr;
        QPushButton *m_loadBtn = nullptr;
        QPushButton *m_closeBtn = nullptr;
        QLabel *m_infoLabel = nullptr;

        bool m_sliderDragging = false;
        int m_totalFrames = 0;
    };

} // namespace est

#endif // EST_PLAYBACKCONTROL_H
