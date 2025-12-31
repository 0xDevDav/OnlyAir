#include "PlaybackControls.h"
#include "Application.h"
#include "VideoPlayer.h"
#include "Translator.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace OnlyAir {

PlaybackControls::PlaybackControls(Application* app, QWidget* parent)
    : QFrame(parent)
    , m_app(app)
    , m_playPauseBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_seekSlider(nullptr)
    , m_volumeSlider(nullptr)
    , m_timeLabel(nullptr)
    , m_volumeIcon(nullptr)
    , m_isSeeking(false)
{
    setupUI();
}

void PlaybackControls::setupUI()
{
    setFixedHeight(70);
    setStyleSheet(R"(
        PlaybackControls {
            background-color: #252525;
            border: 1px solid #404040;
            border-radius: 4px;
        }
        QPushButton {
            background-color: #3d3d3d;
            color: #ffffff;
            border: 1px solid #505050;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover:enabled {
            background-color: #13b7d2;
            color: #1d1d1d;
            border-color: #13b7d2;
        }
        QPushButton:pressed {
            background-color: #0e8fa6;
        }
        QPushButton:disabled {
            background-color: #2a2a2a;
            color: #606060;
        }
        QSlider::groove:horizontal {
            border: 1px solid #404040;
            height: 6px;
            background: #1d1d1d;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #13b7d2;
            border: 1px solid #0e8fa6;
            width: 14px;
            margin: -4px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #1fd4f5;
        }
        QSlider::sub-page:horizontal {
            background: #13b7d2;
            border-radius: 3px;
        }
        QLabel {
            color: #ffffff;
        }
    )");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // Controls row
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(8);

    m_playPauseBtn = new QPushButton(TR("play"));
    m_playPauseBtn->setFixedSize(70, 32);
    m_playPauseBtn->setEnabled(false);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &PlaybackControls::onPlayPauseClicked);
    controlsLayout->addWidget(m_playPauseBtn);

    m_stopBtn = new QPushButton(TR("stop"));
    m_stopBtn->setFixedSize(70, 32);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &PlaybackControls::onStopClicked);
    controlsLayout->addWidget(m_stopBtn);

    controlsLayout->addSpacing(16);

    m_timeLabel = new QLabel("00:00 / 00:00");
    m_timeLabel->setStyleSheet("color: #a0a0a0; font-size: 12px;");
    m_timeLabel->setFixedWidth(100);
    controlsLayout->addWidget(m_timeLabel);

    controlsLayout->addStretch();

    // Volume control
    m_volumeIcon = new QLabel(TR("volume"));
    m_volumeIcon->setStyleSheet("color: #a0a0a0; font-size: 11px;");
    controlsLayout->addWidget(m_volumeIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PlaybackControls::onVolumeChanged);
    controlsLayout->addWidget(m_volumeSlider);

    mainLayout->addLayout(controlsLayout);

    // Seek bar row
    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setValue(0);
    m_seekSlider->setEnabled(false);
    connect(m_seekSlider, &QSlider::sliderPressed, this, &PlaybackControls::onSeekPressed);
    connect(m_seekSlider, &QSlider::sliderReleased, this, &PlaybackControls::onSeekReleased);
    connect(m_seekSlider, &QSlider::sliderMoved, this, &PlaybackControls::onSeekMoved);
    mainLayout->addWidget(m_seekSlider);
}

void PlaybackControls::onPlayPauseClicked()
{
    auto* videoPlayer = m_app->getVideoPlayer();
    if (!videoPlayer || !videoPlayer->isLoaded()) return;

    if (videoPlayer->isPlaying()) {
        emit pauseRequested();
    } else {
        emit playRequested();
    }
}

void PlaybackControls::onStopClicked()
{
    emit stopRequested();
}

void PlaybackControls::onSeekPressed()
{
    m_isSeeking = true;
}

void PlaybackControls::onSeekReleased()
{
    m_isSeeking = false;

    auto* videoPlayer = m_app->getVideoPlayer();
    if (!videoPlayer || !videoPlayer->isLoaded()) return;

    double duration = videoPlayer->getDuration();
    double seekTime = (m_seekSlider->value() / 1000.0) * duration;
    emit seekRequested(seekTime);
}

void PlaybackControls::onSeekMoved(int position)
{
    auto* videoPlayer = m_app->getVideoPlayer();
    if (!videoPlayer || !videoPlayer->isLoaded()) return;

    double duration = videoPlayer->getDuration();
    double seekTime = (position / 1000.0) * duration;

    // Update time label during drag
    m_timeLabel->setText(QString("%1 / %2")
        .arg(formatTime(seekTime))
        .arg(formatTime(duration)));
}

void PlaybackControls::onVolumeChanged(int value)
{
    emit volumeChanged(value / 100.0f);
}

void PlaybackControls::updateState()
{
    auto* videoPlayer = m_app->getVideoPlayer();

    bool hasVideo = videoPlayer && videoPlayer->isLoaded();

    m_playPauseBtn->setEnabled(hasVideo);
    m_stopBtn->setEnabled(hasVideo);
    m_seekSlider->setEnabled(hasVideo);

    if (hasVideo) {
        // Update play/pause button text
        if (videoPlayer->isPlaying()) {
            m_playPauseBtn->setText(TR("pause"));
        } else {
            m_playPauseBtn->setText(TR("play"));
        }

        // Update seek bar (if not dragging)
        if (!m_isSeeking) {
            double currentTime = videoPlayer->getCurrentTime();
            double duration = videoPlayer->getDuration();

            if (duration > 0) {
                int position = (int)((currentTime / duration) * 1000);
                m_seekSlider->setValue(position);
            }

            m_timeLabel->setText(QString("%1 / %2")
                .arg(formatTime(currentTime))
                .arg(formatTime(duration)));
        }
    } else {
        m_playPauseBtn->setText(TR("play"));
        m_seekSlider->setValue(0);
        m_timeLabel->setText("00:00 / 00:00");
    }
}

QString PlaybackControls::formatTime(double seconds)
{
    int totalSeconds = (int)seconds;
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

} // namespace OnlyAir
