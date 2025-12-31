#include "VideoPreviewWidget.h"
#include <QOpenGLShaderProgram>
#include <cstring>

namespace OnlyAir {

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_textureId(0)
    , m_frameReady(false)
    , m_textureInitialized(false)
    , m_videoWidth(0)
    , m_videoHeight(0)
{
    setMinimumSize(320, 240);
}

VideoPreviewWidget::~VideoPreviewWidget()
{
    makeCurrent();
    if (m_textureId) {
        glDeleteTextures(1, &m_textureId);
    }
    doneCurrent();
}

void VideoPreviewWidget::updateFrame(const uint8_t* data, int width, int height, int stride)
{
    QMutexLocker lock(&m_frameMutex);

    // Resize buffer if needed
    if (m_frameBuffer.width() != width || m_frameBuffer.height() != height) {
        m_frameBuffer = QImage(width, height, QImage::Format_RGB888);
    }

    // Copy frame data with stride handling
    for (int y = 0; y < height; y++) {
        memcpy(m_frameBuffer.scanLine(y), data + y * stride, width * 3);
    }

    m_videoWidth = width;
    m_videoHeight = height;
    m_frameReady = true;

    // Request repaint on GUI thread
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void VideoPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Generate texture
    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_textureInitialized = true;
}

void VideoPreviewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    QMutexLocker lock(&m_frameMutex);

    if (!m_frameReady || m_frameBuffer.isNull() || !m_textureInitialized) {
        // Draw placeholder text would require more setup
        // Just show black for now
        return;
    }

    // Update texture with new frame
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_frameBuffer.width(), m_frameBuffer.height(),
                 0, GL_RGB, GL_UNSIGNED_BYTE, m_frameBuffer.bits());

    // Calculate aspect ratio preserving coordinates
    float videoAspect = (float)m_videoWidth / (float)m_videoHeight;
    float widgetAspect = (float)width() / (float)height();

    float x1, y1, x2, y2;

    if (videoAspect > widgetAspect) {
        // Video is wider - fit to width
        x1 = -1.0f;
        x2 = 1.0f;
        float h = widgetAspect / videoAspect;
        y1 = -h;
        y2 = h;
    } else {
        // Video is taller - fit to height
        y1 = -1.0f;
        y2 = 1.0f;
        float w = videoAspect / widgetAspect;
        x1 = -w;
        x2 = w;
    }

    // Enable texturing
    glEnable(GL_TEXTURE_2D);

    // Draw textured quad
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x1, y1);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x2, y1);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x2, y2);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x1, y2);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void VideoPreviewWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

} // namespace OnlyAir
