#include "SceneManager.h"
#include <cstring>
#include <algorithm>

namespace OnlyAir {

SceneManager::SceneManager()
    : m_width(1280)
    , m_height(720)
    , m_hasVideoFrame(false)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_videoStride(0)
    , m_currentScene(SceneType::Black)
    , m_targetScene(SceneType::Black)
    , m_isTransitioning(false)
{
    ensureBlackFrame();
}

SceneManager::~SceneManager()
{
}

void SceneManager::setOutputSize(int width, int height)
{
    m_width = width;
    m_height = height;
    ensureBlackFrame();
}

void SceneManager::ensureBlackFrame()
{
    int size = m_width * m_height * 3;  // BGR24
    if ((int)m_blackFrame.size() != size) {
        m_blackFrame.resize(size, 0);  // All zeros = black
    }
}

void SceneManager::setActiveScene(SceneType scene)
{
    SceneType current = m_currentScene.load();

    if (current == scene) {
        m_targetScene = scene;
        return;
    }

    std::lock_guard<std::mutex> lock(m_transitionMutex);
    m_targetScene = scene;
    m_transitionStart = std::chrono::steady_clock::now();
    m_isTransitioning = true;
}

float SceneManager::getTransitionProgress() const
{
    if (!m_isTransitioning.load()) {
        return m_currentScene.load() == m_targetScene.load() ? 1.0f : 0.0f;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_transitionStart).count();
    float progress = (float)elapsed / (float)TRANSITION_DURATION_MS;
    return std::min(1.0f, std::max(0.0f, progress));
}

void SceneManager::updateVideoFrame(const uint8_t* data, int width, int height, int stride)
{
    std::lock_guard<std::mutex> lock(m_videoFrameMutex);

    int size = height * stride;
    if ((int)m_videoFrame.size() != size) {
        m_videoFrame.resize(size);
    }

    memcpy(m_videoFrame.data(), data, size);
    m_videoWidth = width;
    m_videoHeight = height;
    m_videoStride = stride;
    m_hasVideoFrame = true;

    // Also update output size to match video
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        ensureBlackFrame();
    }
}

void SceneManager::getOutputFrame(std::vector<uint8_t>& outBuffer, int& outWidth, int& outHeight, int& outStride)
{
    std::lock_guard<std::mutex> lock(m_videoFrameMutex);

    outWidth = m_width;
    outHeight = m_height;
    outStride = m_width * 3;

    int size = outHeight * outStride;
    if ((int)outBuffer.size() != size) {
        outBuffer.resize(size);
    }

    // Check transition state
    float progress = getTransitionProgress();
    SceneType target = m_targetScene.load();
    SceneType current = m_currentScene.load();

    // Update transition state
    if (m_isTransitioning.load() && progress >= 1.0f) {
        m_currentScene = target;
        m_isTransitioning = false;
        current = target;
    }

    // Get source frames
    const uint8_t* srcA = nullptr;  // Current scene
    const uint8_t* srcB = nullptr;  // Target scene
    int srcAStride = outStride;
    int srcBStride = outStride;

    if (current == SceneType::Black) {
        srcA = m_blackFrame.data();
        srcAStride = m_width * 3;
    } else if (m_hasVideoFrame) {
        srcA = m_videoFrame.data();
        srcAStride = m_videoStride;
    } else {
        srcA = m_blackFrame.data();
        srcAStride = m_width * 3;
    }

    if (target == SceneType::Black) {
        srcB = m_blackFrame.data();
        srcBStride = m_width * 3;
    } else if (m_hasVideoFrame) {
        srcB = m_videoFrame.data();
        srcBStride = m_videoStride;
    } else {
        srcB = m_blackFrame.data();
        srcBStride = m_width * 3;
    }

    // If not transitioning, just copy current scene
    if (!m_isTransitioning.load() || progress >= 1.0f) {
        const uint8_t* src = (current == SceneType::Black || !m_hasVideoFrame)
                             ? m_blackFrame.data() : m_videoFrame.data();
        int srcStride = (current == SceneType::Black || !m_hasVideoFrame)
                        ? (m_width * 3) : m_videoStride;

        for (int y = 0; y < outHeight; y++) {
            const uint8_t* srcRow = src + y * srcStride;
            uint8_t* dstRow = outBuffer.data() + y * outStride;
            memcpy(dstRow, srcRow, m_width * 3);
        }
    } else {
        // Blend frames during transition with correct strides
        blendFrames(srcA, srcAStride, srcB, srcBStride,
                    outBuffer.data(), outStride, m_width, m_height, progress);
    }
}

void SceneManager::blendFrames(const uint8_t* srcA, int strideA, const uint8_t* srcB, int strideB,
                                uint8_t* dst, int dstStride, int width, int height, float alpha)
{
    // alpha: 0.0 = srcA, 1.0 = srcB
    float invAlpha = 1.0f - alpha;
    int bytesPerRow = width * 3;

    for (int y = 0; y < height; y++) {
        const uint8_t* rowA = srcA + y * strideA;
        const uint8_t* rowB = srcB + y * strideB;
        uint8_t* rowDst = dst + y * dstStride;

        for (int x = 0; x < bytesPerRow; x++) {
            float a = (float)rowA[x];
            float b = (float)rowB[x];
            rowDst[x] = (uint8_t)(a * invAlpha + b * alpha);
        }
    }
}

void SceneManager::processFrame()
{
    if (!m_outputCallback) return;

    std::vector<uint8_t> buffer;
    int width, height, stride;

    getOutputFrame(buffer, width, height, stride);

    m_outputCallback(buffer.data(), width, height, stride);
}

} // namespace OnlyAir
