#include "FrameBuffer.h"
#include <chrono>

namespace OnlyAir {

FrameBuffer::FrameBuffer(size_t capacity)
    : m_capacity(capacity)
    , m_head(0)
    , m_tail(0)
    , m_size(0)
    , m_flushing(false)
{
    m_buffer.resize(capacity);
}

FrameBuffer::~FrameBuffer()
{
    flush();
}

bool FrameBuffer::push(const VideoFrame& frame)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait until not full or flushing
    m_notFull.wait(lock, [this]() {
        return m_size < m_capacity || m_flushing.load();
    });

    if (m_flushing.load()) {
        return false;
    }

    m_buffer[m_tail] = frame;
    m_tail = (m_tail + 1) % m_capacity;
    m_size++;

    lock.unlock();
    m_notEmpty.notify_one();

    return true;
}

bool FrameBuffer::push(VideoFrame&& frame)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait until not full or flushing
    m_notFull.wait(lock, [this]() {
        return m_size < m_capacity || m_flushing.load();
    });

    if (m_flushing.load()) {
        return false;
    }

    m_buffer[m_tail] = std::move(frame);
    m_tail = (m_tail + 1) % m_capacity;
    m_size++;

    lock.unlock();
    m_notEmpty.notify_one();

    return true;
}

bool FrameBuffer::pop(VideoFrame& frame, uint32_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait until not empty or flushing
    auto timeout = std::chrono::milliseconds(timeoutMs);
    bool success = m_notEmpty.wait_for(lock, timeout, [this]() {
        return m_size > 0 || m_flushing.load();
    });

    if (!success || m_flushing.load() || m_size == 0) {
        return false;
    }

    frame = std::move(m_buffer[m_head]);
    m_head = (m_head + 1) % m_capacity;
    m_size--;

    lock.unlock();
    m_notFull.notify_one();

    return true;
}

bool FrameBuffer::peek(VideoFrame& frame) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_size == 0) {
        return false;
    }

    frame = m_buffer[m_head];
    return true;
}

void FrameBuffer::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_head = 0;
    m_tail = 0;
    m_size = 0;

    for (auto& frame : m_buffer) {
        frame.clear();
    }
}

bool FrameBuffer::isEmpty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_size == 0;
}

bool FrameBuffer::isFull() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_size >= m_capacity;
}

size_t FrameBuffer::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_size;
}

void FrameBuffer::flush()
{
    m_flushing.store(true);
    m_notEmpty.notify_all();
    m_notFull.notify_all();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_size = 0;
    }

    m_flushing.store(false);
}

} // namespace OnlyAir
