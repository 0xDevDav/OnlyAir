#include "Application.h"
#include "VideoPlayer.h"
#include "AudioPlayer.h"
#include "SceneManager.h"

#include <cstring>
#include <chrono>

namespace OnlyAir {

Application::Application()
    : m_softcam(nullptr)
    , m_vcamWidth(0)
    , m_vcamHeight(0)
    , m_swsContext(nullptr)
    , m_vcamActive(false)
    , m_vaudioActive(false)
    , m_transmitRunning(false)
{
}

Application::~Application()
{
    shutdown();
}

bool Application::initialize()
{
    // Create components
    m_videoPlayer = std::make_unique<VideoPlayer>();
    m_audioPlayer = std::make_unique<AudioPlayer>();
    m_sceneManager = std::make_unique<SceneManager>();

    // Set default output size (will be updated when video loads)
    m_sceneManager->setOutputSize(1280, 720);

    // Enable virtual camera by default
    m_vcamActive = true;

    // Initialize audio player with VB-Cable if available
    // Try different name variations
    std::wstring cableDevice = AudioPlayer::findDeviceByName(L"CABLE Input");
    if (cableDevice.empty()) {
        cableDevice = AudioPlayer::findDeviceByName(L"VB-Audio");
    }
    if (cableDevice.empty()) {
        cableDevice = AudioPlayer::findDeviceByName(L"Virtual Cable");
    }
    if (!cableDevice.empty()) {
        if (m_audioPlayer->initialize(cableDevice)) {
            m_vaudioActive = true;
        }
    }

    // Start continuous transmission thread
    m_transmitRunning = true;
    m_transmitThread = std::thread(&Application::transmitThread, this);

    return true;
}

void Application::shutdown()
{
    // Stop transmission thread first
    m_transmitRunning = false;
    if (m_transmitThread.joinable()) {
        m_transmitThread.join();
    }

    // Stop video
    if (m_videoPlayer) {
        m_videoPlayer->close();
    }

    // Stop audio
    if (m_audioPlayer) {
        m_audioPlayer->shutdown();
    }

    // Destroy Softcam virtual camera
    if (m_softcam) {
        scDeleteCamera(m_softcam);
        m_softcam = nullptr;
    }

    // Free FFmpeg scaler
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
}

void Application::transmitThread()
{
    using namespace std::chrono;

    // Target ~30 FPS for transmission
    const auto frameInterval = milliseconds(33);

    while (m_transmitRunning.load()) {
        auto frameStart = steady_clock::now();

        if (m_sceneManager) {
            std::vector<uint8_t> buffer;
            int width, height, stride;

            // SceneManager outputs RGB24 (FFmpeg handles all format conversion)
            m_sceneManager->getOutputFrame(buffer, width, height, stride);

            if (!buffer.empty()) {
                // Send to preview callback - RGB24 directly, no conversion needed
                if (m_previewCallback) {
                    m_previewCallback(buffer.data(), width, height, stride);
                }

                // Send to VCam if active
                if (m_vcamActive.load()) {
                    // Softcam requires dimensions to be multiple of 4
                    int vcamWidth = (width / 4) * 4;
                    int vcamHeight = (height / 4) * 4;

                    // Create or recreate Softcam and scaler if dimensions changed
                    if (!m_softcam || m_vcamWidth != vcamWidth || m_vcamHeight != vcamHeight) {
                        if (m_softcam) {
                            scDeleteCamera(m_softcam);
                        }
                        if (m_swsContext) {
                            sws_freeContext(m_swsContext);
                            m_swsContext = nullptr;
                        }

                        m_softcam = scCreateCamera(vcamWidth, vcamHeight, 30.0f);
                        if (m_softcam) {
                            m_vcamWidth = vcamWidth;
                            m_vcamHeight = vcamHeight;
                            m_vcamBuffer.resize(vcamWidth * vcamHeight * 3);

                            // Create FFmpeg scaler for RGB24 -> BGR24 conversion
                            m_swsContext = sws_getContext(
                                vcamWidth, vcamHeight, AV_PIX_FMT_RGB24,
                                vcamWidth, vcamHeight, AV_PIX_FMT_BGR24,
                                SWS_POINT, nullptr, nullptr, nullptr
                            );
                        }
                    }

                    if (m_softcam && m_swsContext && !m_vcamBuffer.empty()) {
                        // Use FFmpeg sws_scale for optimized RGB24 -> BGR24 conversion
                        const uint8_t* srcSlice[1] = { buffer.data() };
                        int srcStride[1] = { stride };
                        uint8_t* dstSlice[1] = { m_vcamBuffer.data() };
                        int dstStride[1] = { vcamWidth * 3 };

                        sws_scale(m_swsContext, srcSlice, srcStride, 0, vcamHeight, dstSlice, dstStride);

                        scSendFrame(m_softcam, m_vcamBuffer.data());
                    }
                }
            }
        }

        // Sleep to maintain frame rate
        auto elapsed = steady_clock::now() - frameStart;
        if (elapsed < frameInterval) {
            std::this_thread::sleep_for(frameInterval - elapsed);
        }
    }
}

void Application::setActiveScene(SceneType scene)
{
    if (m_sceneManager) {
        m_sceneManager->setActiveScene(scene);
    }
}

SceneType Application::getActiveScene() const
{
    return m_sceneManager ? m_sceneManager->getActiveScene() : SceneType::Black;
}

SceneType Application::getCurrentScene() const
{
    return m_sceneManager ? m_sceneManager->getCurrentScene() : SceneType::Black;
}

bool Application::isSceneTransitioning() const
{
    return m_sceneManager ? m_sceneManager->isTransitioning() : false;
}

} // namespace OnlyAir
