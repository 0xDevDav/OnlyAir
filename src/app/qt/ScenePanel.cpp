#include "ScenePanel.h"
#include "Application.h"
#include "SceneManager.h"
#include "Translator.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPen>

namespace OnlyAir {

// ============== SceneThumbnail ==============

SceneThumbnail::SceneThumbnail(const QString& name, QWidget* parent)
    : QFrame(parent)
    , m_name(name)
    , m_previewLabel(nullptr)
    , m_nameLabel(nullptr)
    , m_liveBadge(nullptr)
    , m_openBtn(nullptr)
    , m_active(false)
    , m_live(false)
    , m_showOpenButton(false)
    , m_isBlackScene(false)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 150);

    // No border on outer frame
    setStyleSheet("background: transparent; border: none;");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Preview area with border - expands to fill space
    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setScaledContents(false);
    layout->addWidget(m_previewLabel, 1);

    // Name label at bottom - outside the preview border
    m_nameLabel = new QLabel(m_name, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setFixedHeight(20);
    m_nameLabel->setStyleSheet("color: #aaaaaa; font-size: 11px; background: transparent; border: none;");
    layout->addWidget(m_nameLabel);

    // LIVE badge - overlay on top of preview (top-right)
    m_liveBadge = new QLabel(TR("scene_live"), this);
    m_liveBadge->setAlignment(Qt::AlignCenter);
    m_liveBadge->setFixedSize(45, 20);
    m_liveBadge->setStyleSheet(R"(
        QLabel {
            background-color: #de26b8;
            color: white;
            font-size: 10px;
            font-weight: bold;
            border: 1px solid #f550d0;
            border-radius: 3px;
        }
    )");
    m_liveBadge->hide();

    // Open button - overlay on top of preview (top-left), same height as LIVE badge
    m_openBtn = new QPushButton(TR("menu_open_video").replace("&", ""), this);
    m_openBtn->setFixedSize(95, 20);
    m_openBtn->setCursor(Qt::PointingHandCursor);
    m_openBtn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(45, 45, 45, 220);
            color: #ccc;
            font-size: 10px;
            font-weight: bold;
            border: 1px solid #666;
            border-radius: 3px;
            padding: 0px;
            margin: 0px;
        }
        QPushButton:hover {
            background-color: #13b7d2;
            color: #1d1d1d;
            border-color: #13b7d2;
        }
    )");
    connect(m_openBtn, &QPushButton::clicked, this, &SceneThumbnail::openButtonClicked);
    m_openBtn->hide();  // Hidden by default, shown via setShowOpenButton

    updateStyle();
}

void SceneThumbnail::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        updateStyle();
    }
}

void SceneThumbnail::setLive(bool live)
{
    if (m_live != live) {
        m_live = live;
        updateLiveBadge();
    }
}

void SceneThumbnail::updateLiveBadge()
{
    if (m_liveBadge && m_previewLabel) {
        if (m_live) {
            // Position in top-right corner of preview area
            QRect previewRect = m_previewLabel->geometry();
            int x = previewRect.right() - m_liveBadge->width() - 6;
            int y = previewRect.top() + 6;
            m_liveBadge->move(x, y);
            m_liveBadge->raise();
            m_liveBadge->show();
        } else {
            m_liveBadge->hide();
        }
    }
}

void SceneThumbnail::setShowOpenButton(bool show)
{
    m_showOpenButton = show;
    updateOpenButton();
}

void SceneThumbnail::updateOpenButton()
{
    if (m_openBtn && m_previewLabel) {
        if (m_showOpenButton) {
            // Position in top-left corner of preview area
            QRect previewRect = m_previewLabel->geometry();
            int x = previewRect.left() + 6;
            int y = previewRect.top() + 6;
            m_openBtn->move(x, y);
            m_openBtn->raise();
            m_openBtn->show();
        } else {
            m_openBtn->hide();
        }
    }
}

void SceneThumbnail::setBlackScene()
{
    m_isBlackScene = true;
    m_lastFrame = QImage();  // Clear any video frame
    updatePreview();
}

void SceneThumbnail::updateVideoFrame(const uint8_t* data, int width, int height, int stride)
{
    if (!data || width <= 0 || height <= 0) return;

    QMutexLocker locker(&m_frameMutex);

    // Create QImage from RGB24 data and make a copy
    QImage frame(data, width, height, stride, QImage::Format_RGB888);
    m_lastFrame = frame.copy();

    // Update in main thread
    QMetaObject::invokeMethod(this, [this]() {
        updatePreview();
    }, Qt::QueuedConnection);
}

void SceneThumbnail::updatePreview()
{
    if (!m_previewLabel) return;

    QSize previewSize = m_previewLabel->size();
    if (previewSize.width() < 10 || previewSize.height() < 10) return;

    if (m_isBlackScene) {
        // Black scene - just fill with black
        QPixmap blackPixmap(previewSize);
        blackPixmap.fill(Qt::black);
        m_previewLabel->setPixmap(blackPixmap);
    } else {
        QMutexLocker locker(&m_frameMutex);
        if (!m_lastFrame.isNull()) {
            // Scale video frame to fit preview, keeping aspect ratio
            QImage scaled = m_lastFrame.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // Center in black background
            QPixmap pixmap(previewSize);
            pixmap.fill(Qt::black);
            QPainter p(&pixmap);
            int x = (previewSize.width() - scaled.width()) / 2;
            int y = (previewSize.height() - scaled.height()) / 2;
            p.drawImage(x, y, scaled);

            m_previewLabel->setPixmap(pixmap);
        } else {
            // No video - show placeholder with subtle border
            QPixmap placeholder(previewSize);
            placeholder.fill(QColor("#1a1a1a"));
            QPainter p(&placeholder);
            p.setRenderHint(QPainter::Antialiasing);

            // Draw subtle dashed border
            QPen borderPen(QColor("#404040"));
            borderPen.setWidth(1);
            borderPen.setStyle(Qt::DashLine);
            p.setPen(borderPen);
            p.drawRoundedRect(2, 2, previewSize.width() - 4, previewSize.height() - 4, 3, 3);

            // Draw text
            p.setPen(QColor("#606060"));
            p.setFont(QFont("Arial", 10));
            p.drawText(placeholder.rect(), Qt::AlignCenter, TR("scene_no_video"));
            m_previewLabel->setPixmap(placeholder);
        }
    }
}

void SceneThumbnail::updateStyle()
{
    QString borderColor;
    int borderWidth;

    if (m_active) {
        borderColor = "#13b7d2";  // Cyan for active/selected
        borderWidth = 2;
    } else {
        borderColor = "#404040";  // Gray for inactive
        borderWidth = 1;
    }

    // Apply border to preview label only
    if (m_previewLabel) {
        m_previewLabel->setStyleSheet(QString(
            "background-color: #000000;"
            "border: %1px solid %2;"
            "border-radius: 4px;"
        ).arg(borderWidth).arg(borderColor));
    }
}

void SceneThumbnail::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QFrame::mousePressEvent(event);
}

void SceneThumbnail::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    // Update preview when resized
    updatePreview();
    // Reposition overlays
    updateLiveBadge();
    updateOpenButton();
}

void SceneThumbnail::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    // LIVE badge is now handled by m_liveBadge QLabel overlay
}

// ============== ScenePanel ==============

ScenePanel::ScenePanel(Application* app, QWidget* parent)
    : QFrame(parent)
    , m_app(app)
    , m_blackScene(nullptr)
    , m_videoScene(nullptr)
    , m_videoLoaded(false)
{
    setupUI();
}

void ScenePanel::setupUI()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(R"(
        ScenePanel {
            background-color: #1d1d1d;
            border: none;
        }
    )");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Black scene thumbnail - takes 50% of space
    m_blackScene = new SceneThumbnail(TR("scene_black"), this);
    m_blackScene->setBlackScene();
    m_blackScene->setActive(true);
    m_blackScene->setLive(true);
    connect(m_blackScene, &SceneThumbnail::clicked, this, [this]() {
        emit sceneChangeRequested(0);
    });
    layout->addWidget(m_blackScene, 1);  // Stretch factor 1

    // Video scene thumbnail - takes 50% of space
    m_videoScene = new SceneThumbnail(TR("scene_video"), this);
    m_videoScene->setShowOpenButton(true);  // Show open button overlay
    connect(m_videoScene, &SceneThumbnail::clicked, this, [this]() {
        // Always emit - MainWindow will handle opening file dialog if no video
        emit sceneChangeRequested(1);
    });
    connect(m_videoScene, &SceneThumbnail::openButtonClicked, this, [this]() {
        emit openVideoRequested();
    });
    layout->addWidget(m_videoScene, 1);  // Stretch factor 1
}

void ScenePanel::setVideoLoaded(bool loaded, const QString& filename)
{
    m_videoLoaded = loaded;
    // Always keep enabled - clicking opens file dialog if no video

    if (!loaded) {
        // Clear video frame to show placeholder
        m_videoScene->updateVideoFrame(nullptr, 0, 0, 0);
    }
}

void ScenePanel::updateVideoThumbnail(const uint8_t* data, int width, int height, int stride)
{
    if (m_videoScene) {
        m_videoScene->updateVideoFrame(data, width, height, stride);
    }
}

void ScenePanel::updateState()
{
    if (!m_app) return;

    SceneType currentScene = m_app->getCurrentScene();
    SceneType targetScene = m_app->getActiveScene();
    bool isTransitioning = m_app->isSceneTransitioning();

    // Update active state (which one is selected)
    m_blackScene->setActive(targetScene == SceneType::Black);
    m_videoScene->setActive(targetScene == SceneType::Video);

    // Update live state (which one is actually on air)
    if (isTransitioning) {
        // During transition, both could be partially visible
        m_blackScene->setLive(false);
        m_videoScene->setLive(false);
    } else {
        m_blackScene->setLive(currentScene == SceneType::Black);
        m_videoScene->setLive(currentScene == SceneType::Video);
    }
}

} // namespace OnlyAir
