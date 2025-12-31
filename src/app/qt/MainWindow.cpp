#include "MainWindow.h"
#include "ScenePanel.h"
#include "PlaybackControls.h"
#include "Application.h"
#include "VideoPlayer.h"
#include "AudioPlayer.h"
#include "SceneManager.h"
#include "Translator.h"

#include <QShortcut>
#include <QKeySequence>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFrame>
#include <QApplication>
#include <QPushButton>
#include <QStyle>
#include <QIcon>
#include <QMimeData>
#include <QUrl>

namespace OnlyAir {

MainWindow::MainWindow(Application* app, QWidget* parent)
    : QMainWindow(parent)
    , m_app(app)
    , m_scenePanel(nullptr)
    , m_controls(nullptr)
    , m_updateTimer(nullptr)
{
    setWindowTitle("OnlyAir");
    setWindowIcon(QIcon(":/icons/OnlyAir.ico"));
    resize(960, 600);
    setMinimumSize(700, 400);
    setAcceptDrops(true);

    applyDarkTheme();
    setupCentralWidget();
    setupStatusBar();
    setupShortcuts();

    // Update timer for UI refresh
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateUI);
    m_updateTimer->start(100); // UI update 10Hz is enough
}

MainWindow::~MainWindow()
{
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #1d1d1d;
        }
        QStatusBar {
            background-color: #252525;
            color: #888;
            border-top: 1px solid #333;
        }
        QStatusBar::item {
            border: none;
        }
        QLabel {
            color: #e0e0e0;
        }
        QPushButton {
            background-color: #333;
            color: #e0e0e0;
            border: 1px solid #444;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #13b7d2;
            color: #1d1d1d;
            border-color: #13b7d2;
        }
        QPushButton:pressed {
            background-color: #0e8fa6;
        }
        QPushButton:checked {
            background-color: #13b7d2;
            border-color: #13b7d2;
            color: #1d1d1d;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #333;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #13b7d2;
            width: 14px;
            height: 14px;
            margin: -4px 0;
            border-radius: 7px;
        }
        QSlider::sub-page:horizontal {
            background: #13b7d2;
            border-radius: 3px;
        }
    )");
}

void MainWindow::setupShortcuts()
{
    // Ctrl+O to open video
    QShortcut* openShortcut = new QShortcut(QKeySequence::Open, this);
    connect(openShortcut, &QShortcut::activated, this, &MainWindow::onOpenFile);

    // Ctrl+Q to quit
    QShortcut* quitShortcut = new QShortcut(QKeySequence::Quit, this);
    connect(quitShortcut, &QShortcut::activated, this, &QMainWindow::close);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            QString path = url.toLocalFile().toLower();
            if (path.endsWith(".mp4") || path.endsWith(".avi") ||
                path.endsWith(".mkv") || path.endsWith(".mov") ||
                path.endsWith(".wmv") || path.endsWith(".webm")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString pathLower = path.toLower();
        if (pathLower.endsWith(".mp4") || pathLower.endsWith(".avi") ||
            pathLower.endsWith(".mkv") || pathLower.endsWith(".mov") ||
            pathLower.endsWith(".wmv") || pathLower.endsWith(".webm")) {
            openFile(path);
            return;
        }
    }
}

void MainWindow::setupCentralWidget()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // === SCENE PANEL (with dual previews) ===
    m_scenePanel = new ScenePanel(m_app, this);
    mainLayout->addWidget(m_scenePanel, 1);  // Stretch factor 1 to expand

    // === PLAYBACK CONTROLS ===
    m_controls = new PlaybackControls(m_app, this);
    mainLayout->addWidget(m_controls);

    setCentralWidget(centralWidget);

    // Connect scene panel signals
    connect(m_scenePanel, &ScenePanel::sceneChangeRequested, this, [this](int sceneType) {
        auto* videoPlayer = m_app->getVideoPlayer();

        if (sceneType == 0) {
            // Black scene - only switch if not already on black
            if (m_app->getActiveScene() != SceneType::Black) {
                // First transition to black, THEN reset video
                // So viewers don't see the video jump to first frame
                m_app->setActiveScene(SceneType::Black);
                if (videoPlayer && videoPlayer->isLoaded()) {
                    videoPlayer->stop();
                    // Reset thumbnail to first frame (skip SceneManager update)
                    m_skipSceneManagerUpdate = true;
                    videoPlayer->decodeFirstFrame();
                    m_skipSceneManagerUpdate = false;
                }
            }
        } else {
            // Video scene
            if (!videoPlayer || !videoPlayer->isLoaded()) {
                // No video loaded - open file dialog
                onOpenFile();
            } else if (m_app->getActiveScene() != SceneType::Video) {
                // Video loaded, switch scene - restart from beginning
                videoPlayer->stop();  // Reset to beginning
                videoPlayer->play();  // Start from 0
                m_app->setActiveScene(SceneType::Video);
            }
        }
    });

    // Connect playback controls
    connect(m_controls, &PlaybackControls::playRequested, this, [this]() {
        auto* videoPlayer = m_app->getVideoPlayer();
        if (videoPlayer && videoPlayer->isLoaded()) {
            videoPlayer->play();
            m_app->setActiveScene(SceneType::Video);
        }
    });

    connect(m_controls, &PlaybackControls::pauseRequested, this, [this]() {
        auto* videoPlayer = m_app->getVideoPlayer();
        if (videoPlayer) {
            videoPlayer->pause();
        }
    });

    connect(m_controls, &PlaybackControls::stopRequested, this, [this]() {
        // First transition to black, THEN reset video
        m_app->setActiveScene(SceneType::Black);
        auto* videoPlayer = m_app->getVideoPlayer();
        if (videoPlayer && videoPlayer->isLoaded()) {
            videoPlayer->stop();
            // Reset thumbnail (skip SceneManager update)
            m_skipSceneManagerUpdate = true;
            videoPlayer->decodeFirstFrame();
            m_skipSceneManagerUpdate = false;
        }
    });

    connect(m_controls, &PlaybackControls::seekRequested, this, [this](double time) {
        auto* videoPlayer = m_app->getVideoPlayer();
        if (videoPlayer) {
            videoPlayer->seek(time);
        }
    });

    connect(m_controls, &PlaybackControls::volumeChanged, this, [this](float volume) {
        auto* audioPlayer = m_app->getAudioPlayer();
        if (audioPlayer) {
            audioPlayer->setVolume(volume);
        }
    });

    // Connect open video from scene panel
    connect(m_scenePanel, &ScenePanel::openVideoRequested, this, &MainWindow::onOpenFile);
}

void MainWindow::setupStatusBar()
{
    m_fileInfoLabel = new QLabel(TR("no_video_loaded"));
    statusBar()->addWidget(m_fileInfoLabel, 1);

    m_vcamStatus = new QLabel();
    m_vaudioStatus = new QLabel();
    statusBar()->addPermanentWidget(m_vcamStatus);
    statusBar()->addPermanentWidget(m_vaudioStatus);

    // Info/About button in status bar
    QPushButton* infoBtn = new QPushButton("Info");
    infoBtn->setFixedHeight(20);
    infoBtn->setToolTip(TR("menu_about"));
    infoBtn->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #888;
            border: none;
            font-size: 11px;
            padding: 2px 6px;
        }
        QPushButton:hover {
            color: #13b7d2;
        }
    )");
    connect(infoBtn, &QPushButton::clicked, this, &MainWindow::onAbout);
    statusBar()->addPermanentWidget(infoBtn);
}

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        TR("open_video_title"),
        QString(),
        TR("video_files_filter")
    );

    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::openFile(const QString& path)
{
    auto* videoPlayer = m_app->getVideoPlayer();
    auto* audioPlayer = m_app->getAudioPlayer();

    if (!videoPlayer) return;

    // Stop current playback
    videoPlayer->close();

    // Set audio output format to match WASAPI before opening
    if (audioPlayer && audioPlayer->isInitialized()) {
        videoPlayer->setAudioOutputFormat(audioPlayer->getSampleRate(), audioPlayer->getChannels());
    }

    // Open new file
    if (videoPlayer->open(path.toStdString())) {
        m_currentFilePath = path;

        // VCam callback - send frames to SceneManager and scene thumbnail
        videoPlayer->setVCamFrameCallback(
            [this](const uint8_t* data, int width, int height, int stride, double pts) {
                // Only send to SceneManager if not in thumbnail-only mode
                if (!m_skipSceneManagerUpdate && m_app->getSceneManager()) {
                    m_app->getSceneManager()->updateVideoFrame(data, width, height, stride);
                }
                // Always update video scene thumbnail
                if (m_scenePanel) {
                    m_scenePanel->updateVideoThumbnail(data, width, height, stride);
                }
            }
        );

        // Audio callback
        videoPlayer->setAudioFrameCallback(
            [this](const float* data, int samples, int channels, int sampleRate) {
                auto* ap = m_app->getAudioPlayer();
                if (ap && ap->isInitialized()) {
                    ap->write(data, samples, channels, sampleRate);
                }
            }
        );

        // Update scene panel
        m_scenePanel->setVideoLoaded(true, QFileInfo(path).fileName());

        // Decode and display first frame in thumbnail
        videoPlayer->decodeFirstFrame();

        // Update status bar
        m_fileInfoLabel->setText(QString("%1  |  %2x%3  |  %4 fps")
            .arg(QFileInfo(path).fileName())
            .arg(videoPlayer->getVideoWidth())
            .arg(videoPlayer->getVideoHeight())
            .arg(videoPlayer->getFrameRate(), 0, 'f', 1));
    } else {
        QMessageBox::warning(this, TR("error"),
            QString("%1\n%2").arg(TR("error_open_video")).arg(QString::fromStdString(videoPlayer->getLastError())));
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, TR("about_title"), TR("about_text"));
}

void MainWindow::updateUI()
{
    // Update status indicators
    if (m_app->isVirtualCameraActive()) {
        m_vcamStatus->setText(QString("<span style='color:#13b7d2;'>%1</span>").arg(TR("vcam_on")));
    } else {
        m_vcamStatus->setText(QString("<span style='color:#666;'>%1</span>").arg(TR("vcam_off")));
    }

    if (m_app->isVirtualAudioActive()) {
        m_vaudioStatus->setText(QString("<span style='color:#13b7d2;'>%1</span>").arg(TR("audio_on")));
    } else {
        m_vaudioStatus->setText(QString("<span style='color:#666;'>%1</span>").arg(TR("audio_off")));
    }

    // Update scene panel
    if (m_scenePanel) {
        m_scenePanel->updateState();
    }

    // Update playback controls
    if (m_controls) {
        m_controls->updateState();
    }

    // Check for video end
    auto* videoPlayer = m_app->getVideoPlayer();
    if (videoPlayer && videoPlayer->getState() == PlayerState::EndOfFile) {
        if (m_app->getActiveScene() == SceneType::Video) {
            // First transition to black, THEN reset video
            m_app->setActiveScene(SceneType::Black);
            videoPlayer->stop();  // Reset to beginning (slider goes to 0)
            // Reset thumbnail (skip SceneManager update)
            m_skipSceneManagerUpdate = true;
            videoPlayer->decodeFirstFrame();
            m_skipSceneManagerUpdate = false;
        }
    }
}

} // namespace OnlyAir
