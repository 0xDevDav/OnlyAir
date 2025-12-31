#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMutex>
#include <QImage>

namespace OnlyAir {

class VideoPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget* parent = nullptr);
    ~VideoPreviewWidget();

    // Call from video callback (thread-safe)
    void updateFrame(const uint8_t* data, int width, int height, int stride);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QMutex m_frameMutex;
    QImage m_frameBuffer;
    GLuint m_textureId;
    bool m_frameReady;
    bool m_textureInitialized;
    int m_videoWidth;
    int m_videoHeight;
};

} // namespace OnlyAir
