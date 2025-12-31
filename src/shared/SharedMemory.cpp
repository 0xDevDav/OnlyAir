#include "SharedMemory.h"
#include <cstring>

namespace OnlyAir {

SharedMemory::SharedMemory()
    : m_hMapFile(nullptr)
    , m_hFrameReadyEvent(nullptr)
    , m_hMutex(nullptr)
    , m_pHeader(nullptr)
    , m_pFrameData(nullptr)
    , m_isProducer(false)
{
}

SharedMemory::~SharedMemory()
{
    close();
}

bool SharedMemory::create()
{
    // Create mutex for synchronization
    m_hMutex = CreateMutexW(nullptr, FALSE, FRAME_MUTEX_NAME);
    if (!m_hMutex) {
        return false;
    }

    // Create event for frame signaling
    m_hFrameReadyEvent = CreateEventW(nullptr, FALSE, FALSE, FRAME_READY_EVENT_NAME);
    if (!m_hFrameReadyEvent) {
        close();
        return false;
    }

    // Create shared memory
    m_hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(SHARED_MEMORY_SIZE),
        SHARED_MEMORY_NAME
    );

    if (!m_hMapFile) {
        close();
        return false;
    }

    // Map view of file
    void* pBuf = MapViewOfFile(
        m_hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        SHARED_MEMORY_SIZE
    );

    if (!pBuf) {
        close();
        return false;
    }

    m_pHeader = static_cast<SharedFrameHeader*>(pBuf);
    m_pFrameData = reinterpret_cast<uint8_t*>(pBuf) + sizeof(SharedFrameHeader);
    m_isProducer = true;

    // Initialize header
    std::memset(m_pHeader, 0, sizeof(SharedFrameHeader));
    m_pHeader->magic = SHARED_FRAME_MAGIC;
    m_pHeader->version = SHARED_FRAME_VERSION;
    m_pHeader->isActive = 0;

    return true;
}

bool SharedMemory::open()
{
    // Open existing mutex
    m_hMutex = OpenMutexW(SYNCHRONIZE, FALSE, FRAME_MUTEX_NAME);
    if (!m_hMutex) {
        return false;
    }

    // Open existing event
    m_hFrameReadyEvent = OpenEventW(SYNCHRONIZE, FALSE, FRAME_READY_EVENT_NAME);
    if (!m_hFrameReadyEvent) {
        close();
        return false;
    }

    // Open existing shared memory
    m_hMapFile = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        SHARED_MEMORY_NAME
    );

    if (!m_hMapFile) {
        close();
        return false;
    }

    // Map view of file (read-only for consumer)
    void* pBuf = MapViewOfFile(
        m_hMapFile,
        FILE_MAP_READ,
        0,
        0,
        SHARED_MEMORY_SIZE
    );

    if (!pBuf) {
        close();
        return false;
    }

    m_pHeader = static_cast<SharedFrameHeader*>(pBuf);
    m_pFrameData = reinterpret_cast<uint8_t*>(pBuf) + sizeof(SharedFrameHeader);
    m_isProducer = false;

    // Validate magic number
    if (m_pHeader->magic != SHARED_FRAME_MAGIC) {
        close();
        return false;
    }

    return true;
}

void SharedMemory::close()
{
    if (m_pHeader) {
        if (m_isProducer) {
            m_pHeader->isActive = 0;
        }
        UnmapViewOfFile(m_pHeader);
        m_pHeader = nullptr;
        m_pFrameData = nullptr;
    }

    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
    }

    if (m_hFrameReadyEvent) {
        CloseHandle(m_hFrameReadyEvent);
        m_hFrameReadyEvent = nullptr;
    }

    if (m_hMutex) {
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }
}

bool SharedMemory::writeFrame(const uint8_t* data, uint32_t width, uint32_t height,
                               uint32_t stride, uint32_t format, uint64_t timestamp)
{
    if (!m_pHeader || !m_isProducer || !data) {
        return false;
    }

    uint32_t dataSize = stride * height;
    if (dataSize > MAX_FRAME_SIZE) {
        return false;
    }

    // Lock mutex
    DWORD waitResult = WaitForSingleObject(m_hMutex, 1000);
    if (waitResult != WAIT_OBJECT_0) {
        return false;
    }

    // Update header
    m_pHeader->width = width;
    m_pHeader->height = height;
    m_pHeader->stride = stride;
    m_pHeader->format = format;
    m_pHeader->timestamp = timestamp;
    m_pHeader->dataSize = dataSize;
    m_pHeader->frameNumber++;

    // Copy frame data
    std::memcpy(m_pFrameData, data, dataSize);

    // Release mutex
    ReleaseMutex(m_hMutex);

    // Signal frame ready
    SetEvent(m_hFrameReadyEvent);

    return true;
}

void SharedMemory::setActive(bool active)
{
    if (m_pHeader && m_isProducer) {
        m_pHeader->isActive = active ? 1 : 0;
    }
}

void SharedMemory::setFps(uint32_t fps)
{
    if (m_pHeader && m_isProducer) {
        m_pHeader->fps = fps;
    }
}

bool SharedMemory::readFrame(uint8_t* buffer, size_t bufferSize, SharedFrameHeader* outHeader)
{
    if (!m_pHeader || !buffer) {
        return false;
    }

    // Lock mutex
    DWORD waitResult = WaitForSingleObject(m_hMutex, 100);
    if (waitResult != WAIT_OBJECT_0) {
        return false;
    }

    // Validate
    if (m_pHeader->magic != SHARED_FRAME_MAGIC || m_pHeader->dataSize == 0) {
        ReleaseMutex(m_hMutex);
        return false;
    }

    size_t copySize = (bufferSize < m_pHeader->dataSize) ? bufferSize : m_pHeader->dataSize;

    // Copy header if requested
    if (outHeader) {
        *outHeader = *m_pHeader;
    }

    // Copy frame data
    std::memcpy(buffer, m_pFrameData, copySize);

    ReleaseMutex(m_hMutex);

    return true;
}

bool SharedMemory::waitForFrame(uint32_t timeoutMs)
{
    if (!m_hFrameReadyEvent) {
        return false;
    }

    DWORD result = WaitForSingleObject(m_hFrameReadyEvent, timeoutMs);
    return (result == WAIT_OBJECT_0);
}

const uint8_t* SharedMemory::getFrameData() const
{
    return m_pFrameData;
}

} // namespace OnlyAir
