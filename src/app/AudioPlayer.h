#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>

namespace OnlyAir {

// Audio buffer for queuing
struct AudioBuffer {
    std::vector<float> data;
    int samples;
    int channels;
    int sampleRate;
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Disable copy
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Initialize with specific device (empty for default)
    bool initialize(const std::wstring& deviceName = L"");

    // Shutdown
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return m_isInitialized; }

    // Write audio samples (interleaved float, -1.0 to 1.0)
    bool write(const float* data, int samples, int channels, int sampleRate);

    // Flush audio buffer
    void flush();

    // Get/Set volume (0.0 to 1.0)
    float getVolume() const { return m_volume; }
    void setVolume(float volume);

    // Get available audio devices
    static std::vector<std::wstring> getDevices();

    // Find device by name substring (e.g., "CABLE Input")
    static std::wstring findDeviceByName(const std::wstring& nameSubstring);

    // Get current device name
    const std::wstring& getCurrentDevice() const { return m_deviceName; }

    // Get format info
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }

private:
    // Audio thread function
    void audioThread();

    // Convert sample rate if needed
    void resample(const float* input, int inputSamples, int inputChannels, int inputRate,
                  std::vector<float>& output);

    // COM interfaces
    IMMDeviceEnumerator* m_pEnumerator;
    IMMDevice* m_pDevice;
    IAudioClient* m_pAudioClient;
    IAudioRenderClient* m_pRenderClient;

    // Format
    WAVEFORMATEX* m_pWaveFormat;
    int m_sampleRate;
    int m_channels;
    UINT32 m_bufferFrameCount;

    // State
    std::atomic<bool> m_isInitialized;
    std::atomic<bool> m_running;
    std::atomic<float> m_volume;

    // Device info
    std::wstring m_deviceName;

    // Audio thread
    std::thread m_audioThread;
    HANDLE m_hEvent;

    // Audio queue
    std::queue<AudioBuffer> m_audioQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Pending samples (partial buffer from queue)
    std::vector<float> m_pendingSamples;
    size_t m_pendingOffset;
};

} // namespace OnlyAir
