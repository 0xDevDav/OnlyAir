#pragma once

#include <QFrame>
#include <QLabel>
#include <QImage>
#include <QMutex>
#include <QPushButton>

namespace OnlyAir {

class Application;

// Clickable scene thumbnail widget - expandable
class SceneThumbnail : public QFrame {
    Q_OBJECT

public:
    explicit SceneThumbnail(const QString& name, QWidget* parent = nullptr);

    void setActive(bool active);
    void setLive(bool live);
    void updateVideoFrame(const uint8_t* data, int width, int height, int stride);
    void setBlackScene();
    void setShowOpenButton(bool show);

signals:
    void clicked();
    void openButtonClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateStyle();
    void updatePreview();
    void updateLiveBadge();
    void updateOpenButton();

    QString m_name;
    QLabel* m_previewLabel;
    QLabel* m_nameLabel;
    QLabel* m_liveBadge;
    QPushButton* m_openBtn;
    bool m_active;
    bool m_live;
    bool m_showOpenButton;
    QImage m_frameBuffer;
    QImage m_lastFrame;
    QMutex m_frameMutex;
    bool m_isBlackScene;
};

class ScenePanel : public QFrame {
    Q_OBJECT

public:
    explicit ScenePanel(Application* app, QWidget* parent = nullptr);

    void updateState();
    void setVideoLoaded(bool loaded, const QString& filename = QString());
    void updateVideoThumbnail(const uint8_t* data, int width, int height, int stride);

signals:
    void sceneChangeRequested(int sceneType); // 0 = Black, 1 = Video
    void openVideoRequested();

private:
    void setupUI();

    Application* m_app;
    SceneThumbnail* m_blackScene;
    SceneThumbnail* m_videoScene;
    bool m_videoLoaded;
};

} // namespace OnlyAir
