#include "AudioPlayer.h"
#include <functiondiscoverykeys_devpkey.h>
#include <cmath>
#include <cstring>

#pragma comment(lib, "ole32.lib")

namespace OnlyAir {

AudioPlayer::AudioPlayer()
    : m_pEnumerator(nullptr)
    , m_pDevice(nullptr)
    , m_pAudioClient(nullptr)
    , m_pRenderClient(nullptr)
    , m_pWaveFormat(nullptr)
    , m_sampleRate(44100)
    , m_channels(2)
    , m_bufferFrameCount(0)
    , m_isInitialized(false)
    , m_running(false)
    , m_volume(1.0f)
    , m_hEvent(nullptr)
    , m_pendingOffset(0)
{
}

AudioPlayer::~AudioPlayer()
{
    shutdown();
}

bool AudioPlayer::initialize(const std::wstring& deviceName)
{
    shutdown();

    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }

    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&m_pEnumerator);
    if (FAILED(hr)) {
        return false;
    }

    // Get device
    if (deviceName.empty()) {
        // Use default render device
        hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice);
    } else {
        // Find device by name
        IMMDeviceCollection* pCollection = nullptr;
        hr = m_pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
        if (SUCCEEDED(hr)) {
            UINT count;
            pCollection->GetCount(&count);

            for (UINT i = 0; i < count; i++) {
                IMMDevice* pEndpoint = nullptr;
                hr = pCollection->Item(i, &pEndpoint);
                if (SUCCEEDED(hr)) {
                    IPropertyStore* pProps = nullptr;
                    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
                    if (SUCCEEDED(hr)) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                        if (SUCCEEDED(hr)) {
                            std::wstring name = varName.pwszVal;
                            if (name.find(deviceName) != std::wstring::npos) {
                                m_pDevice = pEndpoint;
                                m_deviceName = name;
                                pEndpoint = nullptr;  // Don't release
                            }
                            PropVariantClear(&varName);
                        }
                        pProps->Release();
                    }
                    if (pEndpoint) pEndpoint->Release();
                }
                if (m_pDevice) break;
            }
            pCollection->Release();
        }

        if (!m_pDevice) {
            // Fallback to default
            hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice);
        }
    }

    if (FAILED(hr) || !m_pDevice) {
        shutdown();
        return false;
    }

    // Activate audio client
    hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_pAudioClient);
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Get mix format
    hr = m_pAudioClient->GetMixFormat(&m_pWaveFormat);
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    m_sampleRate = m_pWaveFormat->nSamplesPerSec;
    m_channels = m_pWaveFormat->nChannels;

    // Create event for buffer notification
    m_hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!m_hEvent) {
        shutdown();
        return false;
    }

    // Initialize audio client in event-driven mode
    REFERENCE_TIME hnsBufferDuration = 200000;  // 20ms buffer

    hr = m_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsBufferDuration,
        0,
        m_pWaveFormat,
        nullptr
    );

    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Set event handle
    hr = m_pAudioClient->SetEventHandle(m_hEvent);
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Get buffer size
    hr = m_pAudioClient->GetBufferSize(&m_bufferFrameCount);
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Get render client
    hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderClient);
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Start audio client
    hr = m_pAudioClient->Start();
    if (FAILED(hr)) {
        shutdown();
        return false;
    }

    // Start audio thread
    m_running = true;
    m_isInitialized = true;
    m_audioThread = std::thread(&AudioPlayer::audioThread, this);

    return true;
}

void AudioPlayer::shutdown()
{
    m_running = false;
    m_queueCondition.notify_all();

    if (m_audioThread.joinable()) {
        m_audioThread.join();
    }

    if (m_pAudioClient) {
        m_pAudioClient->Stop();
    }

    if (m_pRenderClient) {
        m_pRenderClient->Release();
        m_pRenderClient = nullptr;
    }

    if (m_pAudioClient) {
        m_pAudioClient->Release();
        m_pAudioClient = nullptr;
    }

    if (m_pWaveFormat) {
        CoTaskMemFree(m_pWaveFormat);
        m_pWaveFormat = nullptr;
    }

    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }

    if (m_hEvent) {
        CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    m_isInitialized = false;

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_audioQueue.empty()) {
            m_audioQueue.pop();
        }
    }

    m_pendingSamples.clear();
    m_pendingOffset = 0;
}

bool AudioPlayer::write(const float* data, int samples, int channels, int sampleRate)
{
    if (!m_isInitialized || !data || samples <= 0) {
        return false;
    }

    AudioBuffer buffer;
    buffer.samples = samples;
    buffer.channels = channels;
    buffer.sampleRate = sampleRate;
    buffer.data.assign(data, data + samples * channels);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        // Limit queue size to prevent memory issues
        if (m_audioQueue.size() > 100) {
            return false;  // Drop audio if queue is too full
        }

        m_audioQueue.push(std::move(buffer));
    }

    m_queueCondition.notify_one();
    return true;
}

void AudioPlayer::flush()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_audioQueue.empty()) {
        m_audioQueue.pop();
    }
    m_pendingSamples.clear();
    m_pendingOffset = 0;
}

void AudioPlayer::setVolume(float volume)
{
    m_volume = (volume < 0.0f) ? 0.0f : ((volume > 1.0f) ? 1.0f : volume);
}

void AudioPlayer::audioThread()
{
    while (m_running.load()) {
        // Wait for buffer event
        DWORD waitResult = WaitForSingleObject(m_hEvent, 100);

        if (!m_running.load()) break;

        if (waitResult != WAIT_OBJECT_0) {
            continue;
        }

        // Get current buffer padding
        UINT32 numFramesPadding;
        HRESULT hr = m_pAudioClient->GetCurrentPadding(&numFramesPadding);
        if (FAILED(hr)) continue;

        UINT32 numFramesAvailable = m_bufferFrameCount - numFramesPadding;
        if (numFramesAvailable == 0) continue;

        // Get buffer
        BYTE* pData;
        hr = m_pRenderClient->GetBuffer(numFramesAvailable, &pData);
        if (FAILED(hr)) continue;

        // Fill buffer with audio data
        float* pFloatData = (float*)pData;
        size_t samplesNeeded = numFramesAvailable * m_channels;
        size_t samplesFilled = 0;

        float volume = m_volume.load();

        // First, use pending samples if any
        while (m_pendingOffset < m_pendingSamples.size() && samplesFilled < samplesNeeded) {
            pFloatData[samplesFilled++] = m_pendingSamples[m_pendingOffset++] * volume;
        }

        if (m_pendingOffset >= m_pendingSamples.size()) {
            m_pendingSamples.clear();
            m_pendingOffset = 0;
        }

        // Then, get more from queue
        while (samplesFilled < samplesNeeded) {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            if (m_audioQueue.empty()) {
                lock.unlock();
                // Fill remaining with silence
                while (samplesFilled < samplesNeeded) {
                    pFloatData[samplesFilled++] = 0.0f;
                }
                break;
            }

            AudioBuffer& buffer = m_audioQueue.front();

            // Resample if needed
            if (buffer.sampleRate != m_sampleRate || buffer.channels != m_channels) {
                resample(buffer.data.data(), buffer.samples, buffer.channels, buffer.sampleRate,
                         m_pendingSamples);
            } else {
                m_pendingSamples = std::move(buffer.data);
            }

            m_audioQueue.pop();
            lock.unlock();

            m_pendingOffset = 0;

            // Copy to output
            while (m_pendingOffset < m_pendingSamples.size() && samplesFilled < samplesNeeded) {
                pFloatData[samplesFilled++] = m_pendingSamples[m_pendingOffset++] * volume;
            }

            if (m_pendingOffset >= m_pendingSamples.size()) {
                m_pendingSamples.clear();
                m_pendingOffset = 0;
            }
        }

        // Release buffer
        hr = m_pRenderClient->ReleaseBuffer(numFramesAvailable, 0);
    }
}

void AudioPlayer::resample(const float* input, int inputSamples, int inputChannels, int inputRate,
                           std::vector<float>& output)
{
    // Simple linear resampling
    double ratio = (double)m_sampleRate / inputRate;
    int outputSamples = (int)(inputSamples * ratio);

    output.resize(outputSamples * m_channels);

    for (int i = 0; i < outputSamples; i++) {
        double srcPos = i / ratio;
        int srcIdx = (int)srcPos;
        double frac = srcPos - srcIdx;

        for (int c = 0; c < m_channels; c++) {
            int srcChannel = (c < inputChannels) ? c : 0;

            float v0 = (srcIdx < inputSamples) ? input[srcIdx * inputChannels + srcChannel] : 0.0f;
            float v1 = (srcIdx + 1 < inputSamples) ? input[(srcIdx + 1) * inputChannels + srcChannel] : v0;

            output[i * m_channels + c] = (float)(v0 + (v1 - v0) * frac);
        }
    }
}

std::vector<std::wstring> AudioPlayer::getDevices()
{
    std::vector<std::wstring> devices;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    if (SUCCEEDED(hr)) {
        IMMDeviceCollection* pCollection = nullptr;
        hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);

        if (SUCCEEDED(hr)) {
            UINT count;
            pCollection->GetCount(&count);

            for (UINT i = 0; i < count; i++) {
                IMMDevice* pEndpoint = nullptr;
                hr = pCollection->Item(i, &pEndpoint);

                if (SUCCEEDED(hr)) {
                    IPropertyStore* pProps = nullptr;
                    hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);

                    if (SUCCEEDED(hr)) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);

                        if (SUCCEEDED(hr)) {
                            devices.push_back(varName.pwszVal);
                            PropVariantClear(&varName);
                        }

                        pProps->Release();
                    }
                    pEndpoint->Release();
                }
            }
            pCollection->Release();
        }
        pEnumerator->Release();
    }

    if (comInitialized) {
        CoUninitialize();
    }

    return devices;
}

std::wstring AudioPlayer::findDeviceByName(const std::wstring& nameSubstring)
{
    auto devices = getDevices();
    for (const auto& device : devices) {
        if (device.find(nameSubstring) != std::wstring::npos) {
            return device;
        }
    }
    return L"";
}

} // namespace OnlyAir
