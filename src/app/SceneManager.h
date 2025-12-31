#pragma once

#include <cstdint>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <functional>

namespace OnlyAir {

enum class SceneType {
    Black,
    Video
};

// Callback for blended frame output
using BlendedFrameCallback = std::function<void(const uint8_t* data, int width, int height, int stride)>;

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    // Set output dimensions (call before using)
    void setOutputSize(int width, int height);

    // Scene control
    void setActiveScene(SceneType scene);
    SceneType getActiveScene() const { return m_targetScene.load(); }
    SceneType getCurrentScene() const { return m_currentScene.load(); }

    // Check if transitioning
    bool isTransitioning() const { return m_isTransitioning.load(); }
    float getTransitionProgress() const;

    // Update video frame (call from video callback)
    void updateVideoFrame(const uint8_t* data, int width, int height, int stride);

    // Get current output frame (blends if transitioning)
    // Returns the blended frame for VCam
    void getOutputFrame(std::vector<uint8_t>& outBuffer, int& outWidth, int& outHeight, int& outStride);

    // Set callback for frame output
    void setOutputCallback(BlendedFrameCallback callback) { m_outputCallback = callback; }

    // Process and send frame (call this regularly, e.g., at 30fps)
    void processFrame();

    // Transition duration in ms
    static constexpr int TRANSITION_DURATION_MS = 300;

private:
    // Create black frame
    void ensureBlackFrame();

    // Blend two frames
    void blendFrames(const uint8_t* srcA, int strideA, const uint8_t* srcB, int strideB,
                     uint8_t* dst, int dstStride, int width, int height, float alpha);

    // Output dimensions
    int m_width;
    int m_height;

    // Black frame buffer (BGR24)
    std::vector<uint8_t> m_blackFrame;

    // Video frame buffer (BGR24)
    std::vector<uint8_t> m_videoFrame;
    std::mutex m_videoFrameMutex;
    bool m_hasVideoFrame;
    int m_videoWidth;
    int m_videoHeight;
    int m_videoStride;

    // Blended output buffer
    std::vector<uint8_t> m_outputBuffer;

    // Scene state
    std::atomic<SceneType> m_currentScene;
    std::atomic<SceneType> m_targetScene;

    // Transition state
    std::atomic<bool> m_isTransitioning;
    std::chrono::steady_clock::time_point m_transitionStart;
    std::mutex m_transitionMutex;

    // Output callback
    BlendedFrameCallback m_outputCallback;
};

} // namespace OnlyAir
