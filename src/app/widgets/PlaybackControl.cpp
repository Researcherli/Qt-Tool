#include "widgets/PlaybackControl.h"

#include "services/DataLogger.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QToolButton>

namespace est
{

    PlaybackControl::PlaybackControl(QWidget *parent)
        : QWidget(parent)
    {
        setupUi();
    }

    void PlaybackControl::setupUi()
    {
        setObjectName(QStringLiteral("playbackControl"));
        setStyleSheet(
            QStringLiteral("QWidget#playbackControl {"
                           "  background: #F5F5F5;"
                           "  border-top: 1px solid #E0E0E0;"
                           "  padding: 4px 8px;"
                           "}"));

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(6);

        // 加载文件
        m_loadBtn = new QPushButton(QStringLiteral("📂 加载"), this);
        m_loadBtn->setStyleSheet(
            QStringLiteral("QPushButton { padding: 4px 12px; font-size: 12px;"
                           "  border: 1px solid #BDBDBD; border-radius: 3px;"
                           "  background: #FFF; }"
                           "QPushButton:hover { background: #F0F0F0; }"));
        layout->addWidget(m_loadBtn);

        layout->addSpacing(8);

        // 播放/暂停
        m_playBtn = new QToolButton(this);
        m_playBtn->setText(QStringLiteral("▶"));
        m_playBtn->setEnabled(false);
        m_playBtn->setStyleSheet(
            QStringLiteral("QToolButton { font-size: 16px; padding: 2px 8px;"
                           "  border: none; border-radius: 3px; }"
                           "QToolButton:hover { background: #E0E0E0; }"
                           "QToolButton:disabled { color: #BDBDBD; }"));
        layout->addWidget(m_playBtn);

        // 停止
        m_stopBtn = new QToolButton(this);
        m_stopBtn->setText(QStringLiteral("⏹"));
        m_stopBtn->setEnabled(false);
        m_stopBtn->setStyleSheet(
            QStringLiteral("QToolButton { font-size: 14px; padding: 2px 8px;"
                           "  border: none; border-radius: 3px; }"
                           "QToolButton:hover { background: #E0E0E0; }"
                           "QToolButton:disabled { color: #BDBDBD; }"));
        layout->addWidget(m_stopBtn);

        layout->addSpacing(8);

        // 进度条
        m_progressSlider = new QSlider(Qt::Horizontal, this);
        m_progressSlider->setRange(0, 1000);
        m_progressSlider->setValue(0);
        m_progressSlider->setEnabled(false);
        m_progressSlider->setFixedHeight(16);
        layout->addWidget(m_progressSlider, 1);

        // 信息
        m_infoLabel = new QLabel(QStringLiteral("未加载"), this);
        m_infoLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #888; min-width: 120px;"));
        layout->addWidget(m_infoLabel);

        // 变速
        m_speedBtn = new QPushButton(QStringLiteral("1.0x"), this);
        m_speedBtn->setEnabled(false);
        m_speedBtn->setStyleSheet(
            QStringLiteral("QPushButton { padding: 2px 10px; font-size: 11px;"
                           "  border: 1px solid #BDBDBD; border-radius: 3px;"
                           "  background: #FFF; min-width: 40px; }"
                           "QPushButton:hover { background: #F0F0F0; }"
                           "QPushButton:disabled { color: #BDBDBD; }"));
        layout->addWidget(m_speedBtn);

        // 关闭
        m_closeBtn = new QPushButton(QStringLiteral("✕"), this);
        m_closeBtn->setStyleSheet(
            QStringLiteral("QPushButton { font-size: 14px; padding: 2px 8px;"
                           "  border: none; border-radius: 3px; color: #888; }"
                           "QPushButton:hover { background: #FFCDD2; color: #D32F2F; }"));
        layout->addWidget(m_closeBtn);

        // 连接
        connect(m_loadBtn, &QPushButton::clicked,
                this, &PlaybackControl::loadFileRequested);
        connect(m_closeBtn, &QPushButton::clicked,
                this, &PlaybackControl::closeRequested);
        connect(m_playBtn, &QToolButton::clicked,
                this, &PlaybackControl::onPlayPause);
        connect(m_stopBtn, &QToolButton::clicked,
                this, &PlaybackControl::onStop);
        connect(m_speedBtn, &QPushButton::clicked,
                this, &PlaybackControl::onSpeedChanged);
        connect(m_progressSlider, &QSlider::sliderPressed,
                this, &PlaybackControl::onSliderPressed);
        connect(m_progressSlider, &QSlider::sliderReleased,
                this, &PlaybackControl::onSliderReleased);

        hide();
    }

    void PlaybackControl::bindPlayer(DataPlayer *player)
    {
        if (m_player != nullptr)
            disconnect(m_player, nullptr, this, nullptr);

        m_player = player;
        if (m_player == nullptr)
            return;

        connect(player, &DataPlayer::fileLoaded, this, [this](const QString &, int frames)
                {
            m_totalFrames = frames;
            m_infoLabel->setText(QStringLiteral("%1 帧").arg(frames));
            m_progressSlider->setValue(0);
            m_progressSlider->setEnabled(true);
            m_playBtn->setEnabled(true);
            m_stopBtn->setEnabled(true);
            m_speedBtn->setEnabled(true);
            show(); });

        connect(player, &DataPlayer::playbackProgress,
                this, &PlaybackControl::onProgressUpdated);
        connect(player, &DataPlayer::playbackFinished,
                this, &PlaybackControl::onPlaybackFinished);
        connect(player, &DataPlayer::playbackStarted, this, [this]()
                { m_playBtn->setText(QStringLiteral("⏸")); });
        connect(player, &DataPlayer::playbackPaused, this, [this]()
                { m_playBtn->setText(QStringLiteral("▶")); });
        connect(player, &DataPlayer::playbackStopped, this, [this]()
                {
                    m_playBtn->setText(QStringLiteral("▶"));
                    m_progressSlider->setValue(0);
                    if (m_totalFrames > 0)
                        m_infoLabel->setText(QStringLiteral("0 / %1 帧 (0%)").arg(m_totalFrames));
                });
    }

    void PlaybackControl::onPlayPause()
    {
        if (!m_player)
            return;
        if (m_player->isPlaying() && !m_player->isPaused())
            m_player->pause();
        else
            m_player->play();
    }

    void PlaybackControl::onStop()
    {
        if (m_player)
            m_player->stop();
        m_playBtn->setText(QStringLiteral("▶"));
        m_progressSlider->setValue(0);
    }

    void PlaybackControl::onSpeedChanged()
    {
        if (!m_player)
            return;

        static const double speeds[] = {0.5, 1.0, 2.0, 5.0, 10.0};
        static int idx = 1;
        idx = (idx + 1) % 5;
        m_player->setSpeed(speeds[idx]);
        m_speedBtn->setText(QStringLiteral("%1x").arg(speeds[idx], 0, 'f', 1));
    }

    void PlaybackControl::onSliderPressed()
    {
        m_sliderDragging = true;
    }

    void PlaybackControl::onSliderReleased()
    {
        m_sliderDragging = false;
        if (m_player)
        {
            const double progress = m_progressSlider->value() / 1000.0;
            m_player->seekTo(progress);
        }
    }

    void PlaybackControl::onProgressUpdated(int current, int total, double progress)
    {
        if (!m_sliderDragging)
            m_progressSlider->setValue(static_cast<int>(progress * 1000));

        if (total > 0)
            m_infoLabel->setText(QStringLiteral("%1 / %2 帧 (%3%)")
                                     .arg(current)
                                     .arg(total)
                                     .arg(static_cast<int>(progress * 100)));
    }

    void PlaybackControl::onPlaybackFinished()
    {
        m_playBtn->setText(QStringLiteral("▶"));
        m_progressSlider->setValue(1000);
    }

} // namespace est
