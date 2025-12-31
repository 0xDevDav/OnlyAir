#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <functional>

// Softcam API
#include <softcam/softcam.h>

// FFmpeg for color conversion
extern "C" {
#include <libswscale/swscale.h>
}

#include "SceneManager.h"

namespace OnlyAir {

class VideoPlayer;
class AudioPlayer;

// Callback type for preview frames (RGB24)
using PreviewFrameCallback = std::function<void(const uint8_t* data, int width, int height, int stride)>;

class Application {
public:
    Application();
    ~Application();

    // Disable copy
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Initialize application
    bool initialize();

    // Shutdown
    void shutdown();

    // Get components
    VideoPlayer* getVideoPlayer() { return m_videoPlayer.get(); }
    AudioPlayer* getAudioPlayer() { return m_audioPlayer.get(); }
    SceneManager* getSceneManager() { return m_sceneManager.get(); }

    // Status
    bool isVirtualCameraActive() const { return m_vcamActive; }
    bool isVirtualAudioActive() const { return m_vaudioActive; }
    void setVirtualCameraActive(bool active) { m_vcamActive = active; }
    void setVirtualAudioActive(bool active) { m_vaudioActive = active; }

    // Scene management
    void setActiveScene(SceneType scene);
    SceneType getActiveScene() const;
    SceneType getCurrentScene() const;
    bool isSceneTransitioning() const;

    // Preview callback - receives composited output frame (same as VCam)
    void setPreviewCallback(PreviewFrameCallback callback) { m_previewCallback = callback; }

private:
    // Preview callback
    PreviewFrameCallback m_previewCallback;
    // Components
    std::unique_ptr<VideoPlayer> m_videoPlayer;
    std::unique_ptr<AudioPlayer> m_audioPlayer;
    std::unique_ptr<SceneManager> m_sceneManager;

    // Softcam virtual camera
    scCamera m_softcam;
    std::vector<uint8_t> m_vcamBuffer;
    int m_vcamWidth;
    int m_vcamHeight;

    // FFmpeg color converter (RGB24 -> BGR24 for Softcam)
    SwsContext* m_swsContext;

    // Status
    std::atomic<bool> m_vcamActive;
    std::atomic<bool> m_vaudioActive;

    // Continuous transmission thread
    std::thread m_transmitThread;
    std::atomic<bool> m_transmitRunning;
    void transmitThread();
};

} // namespace OnlyAir
