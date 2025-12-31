#pragma once

#include <Windows.h>
#include <string>
#include <cstdint>
#include <mutex>

namespace OnlyAir {

// Shared memory structure for video frames
struct SharedFrameHeader {
    uint32_t magic;              // Magic number for validation (0x4F43414D = "OCAM")
    uint32_t version;            // Protocol version
    uint32_t width;              // Frame width
    uint32_t height;             // Frame height
    uint32_t stride;             // Bytes per row
    uint32_t format;             // Pixel format (0 = RGB24, 1 = RGB32, 2 = YUV420)
    uint64_t frameNumber;        // Current frame number
    uint64_t timestamp;          // Timestamp in microseconds
    uint32_t dataSize;           // Size of frame data in bytes
    uint32_t isActive;           // Whether the source is active
    uint32_t fps;                // Frames per second * 100 (e.g., 3000 = 30fps)
    uint32_t reserved[5];        // Reserved for future use
};

constexpr uint32_t SHARED_FRAME_MAGIC = 0x4F43414D;  // "OCAM"
constexpr uint32_t SHARED_FRAME_VERSION = 1;
constexpr size_t MAX_FRAME_WIDTH = 1920;
constexpr size_t MAX_FRAME_HEIGHT = 1080;
constexpr size_t MAX_FRAME_SIZE = MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT * 4;  // RGBA
constexpr size_t SHARED_MEMORY_SIZE = sizeof(SharedFrameHeader) + MAX_FRAME_SIZE;

constexpr const wchar_t* SHARED_MEMORY_NAME = L"OnlyAirSharedFrame";
constexpr const wchar_t* FRAME_READY_EVENT_NAME = L"OnlyAirFrameReady";
constexpr const wchar_t* FRAME_MUTEX_NAME = L"OnlyAirFrameMutex";

class SharedMemory {
public:
    SharedMemory();
    ~SharedMemory();

    // Disable copy
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    // Create shared memory (for producer/app)
    bool create();

    // Open existing shared memory (for consumer/virtual camera)
    bool open();

    // Close shared memory
    void close();

    // Check if connected
    bool isConnected() const { return m_pHeader != nullptr; }

    // Producer methods
    bool writeFrame(const uint8_t* data, uint32_t width, uint32_t height,
                    uint32_t stride, uint32_t format, uint64_t timestamp);
    void setActive(bool active);
    void setFps(uint32_t fps);

    // Consumer methods
    bool readFrame(uint8_t* buffer, size_t bufferSize, SharedFrameHeader* outHeader = nullptr);
    bool waitForFrame(uint32_t timeoutMs = 100);
    const SharedFrameHeader* getHeader() const { return m_pHeader; }

    // Get frame data pointer (for zero-copy read)
    const uint8_t* getFrameData() const;

private:
    HANDLE m_hMapFile;
    HANDLE m_hFrameReadyEvent;
    HANDLE m_hMutex;
    SharedFrameHeader* m_pHeader;
    uint8_t* m_pFrameData;
    bool m_isProducer;
};

} // namespace OnlyAir
