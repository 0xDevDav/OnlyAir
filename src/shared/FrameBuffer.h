#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace OnlyAir {

// Pixel formats
enum class PixelFormat : uint32_t {
    RGB24 = 0,
    RGB32 = 1,
    YUV420P = 2,
    BGR24 = 3,
    BGRA32 = 4
};

// Frame data structure
struct VideoFrame {
    std::vector<uint8_t> data;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    PixelFormat format = PixelFormat::RGB24;
    uint64_t timestamp = 0;      // In microseconds
    uint64_t frameNumber = 0;
    double pts = 0.0;            // Presentation timestamp in seconds

    size_t getDataSize() const {
        return stride * height;
    }

    void allocate(uint32_t w, uint32_t h, PixelFormat fmt) {
        width = w;
        height = h;
        format = fmt;

        switch (fmt) {
            case PixelFormat::RGB24:
            case PixelFormat::BGR24:
                stride = ((w * 3 + 3) / 4) * 4;  // Align to 4 bytes
                break;
            case PixelFormat::RGB32:
            case PixelFormat::BGRA32:
                stride = w * 4;
                break;
            case PixelFormat::YUV420P:
                stride = w;  // Y plane stride
                break;
        }

        size_t size;
        if (fmt == PixelFormat::YUV420P) {
            size = w * h * 3 / 2;  // Y + U/4 + V/4
        } else {
            size = stride * h;
        }

        data.resize(size);
    }

    void clear() {
        data.clear();
        width = height = stride = 0;
        timestamp = frameNumber = 0;
        pts = 0.0;
    }
};

// Thread-safe circular buffer for video frames
class FrameBuffer {
public:
    explicit FrameBuffer(size_t capacity = 8);
    ~FrameBuffer();

    // Disable copy
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;

    // Push a frame (producer)
    bool push(const VideoFrame& frame);
    bool push(VideoFrame&& frame);

    // Pop a frame (consumer)
    bool pop(VideoFrame& frame, uint32_t timeoutMs = 100);

    // Peek at front frame without removing
    bool peek(VideoFrame& frame) const;

    // Clear all frames
    void clear();

    // Check if empty/full
    bool isEmpty() const;
    bool isFull() const;

    // Get current size
    size_t size() const;
    size_t capacity() const { return m_capacity; }

    // Flush - clear and signal waiting consumers
    void flush();

private:
    std::vector<VideoFrame> m_buffer;
    size_t m_capacity;
    size_t m_head;
    size_t m_tail;
    size_t m_size;

    mutable std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    std::atomic<bool> m_flushing;
};

} // namespace OnlyAir
