#pragma once

#include <QFrame>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

namespace OnlyAir {

class Application;

class PlaybackControls : public QFrame {
    Q_OBJECT

public:
    explicit PlaybackControls(Application* app, QWidget* parent = nullptr);

    void updateState();

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void seekRequested(double time);
    void volumeChanged(float volume);

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onSeekPressed();
    void onSeekReleased();
    void onSeekMoved(int position);
    void onVolumeChanged(int value);

private:
    void setupUI();
    QString formatTime(double seconds);

    Application* m_app;
    QPushButton* m_playPauseBtn;
    QPushButton* m_stopBtn;
    QSlider* m_seekSlider;
    QSlider* m_volumeSlider;
    QLabel* m_timeLabel;
    QLabel* m_volumeIcon;
    bool m_isSeeking;
};

} // namespace OnlyAir
