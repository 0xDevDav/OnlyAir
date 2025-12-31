#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/display.h>
}

namespace OnlyAir {

// Video frame callback
using VideoFrameCallback = std::function<void(const uint8_t* data, int width, int height,
                                               int stride, double pts)>;

// Audio frame callback (interleaved PCM float)
using AudioFrameCallback = std::function<void(const float* data, int samples,
                                               int channels, int sampleRate)>;

// Player state
enum class PlayerState {
    Idle,
    Loading,
    Playing,
    Paused,
    Stopped,
    EndOfFile,
    Error
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Disable copy
    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    // File operations
    bool open(const std::string& filePath);
    void close();

    // Playback control
    void play();
    void pause();
    void stop();
    void seek(double timeSeconds);

    // Decode and display first frame without starting playback
    void decodeFirstFrame();

    // State
    PlayerState getState() const { return m_state.load(); }
    bool isPlaying() const { return m_state.load() == PlayerState::Playing; }
    bool isPaused() const { return m_state.load() == PlayerState::Paused; }
    bool isLoaded() const { return m_formatContext != nullptr; }

    // Media info
    double getDuration() const { return m_duration; }
    double getCurrentTime() const { return m_currentTime.load(); }
    int getVideoWidth() const { return m_videoWidth; }
    int getVideoHeight() const { return m_videoHeight; }
    double getFrameRate() const { return m_frameRate; }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    int getAudioChannels() const { return m_audioChannels; }
    bool hasVideo() const { return m_videoStreamIndex >= 0; }
    bool hasAudio() const { return m_audioStreamIndex >= 0; }

    // Callbacks
    void setVideoFrameCallback(VideoFrameCallback callback) { m_videoCallback = callback; }
    void setVCamFrameCallback(VideoFrameCallback callback) { m_vcamCallback = callback; }
    void setAudioFrameCallback(AudioFrameCallback callback) { m_audioCallback = callback; }

    // Set audio output format (must be called before play, reconfigures resampler)
    void setAudioOutputFormat(int sampleRate, int channels);

    // Get last error
    const std::string& getLastError() const { return m_lastError; }

private:
    // Decoding thread function
    void decodeThread();

    // Decode video frame
    bool decodeVideoFrame();

    // Decode audio frame
    bool decodeAudioFrame();

    // FFmpeg context
    AVFormatContext* m_formatContext;
    AVCodecContext* m_videoCodecContext;
    AVCodecContext* m_audioCodecContext;
    SwsContext* m_swsContext;
    SwrContext* m_swrContext;

    // Stream indices
    int m_videoStreamIndex;
    int m_audioStreamIndex;

    // Frames
    AVFrame* m_videoFrame;
    AVFrame* m_audioFrame;
    AVFrame* m_rgbFrame;

    // Packet
    AVPacket* m_packet;

    // Converted video buffer (RGB for preview)
    uint8_t* m_rgbBuffer;
    int m_rgbBufferSize;

    // Preview filter graph (transpose + format=rgb24)
    AVFilterGraph* m_previewFilterGraph;
    AVFilterContext* m_previewBuffersrcCtx;
    AVFilterContext* m_previewBuffersinkCtx;
    AVFrame* m_previewFrame;

    // VCam filter graph (transpose + hflip + format=rgb24)
    AVFilterGraph* m_vcamFilterGraph;
    AVFilterContext* m_vcamBuffersrcCtx;
    AVFilterContext* m_vcamBuffersinkCtx;
    AVFrame* m_vcamFrame;

    // Converted audio buffer
    float* m_audioBuffer;
    int m_audioBufferSize;

    // Media info
    int m_videoWidth;
    int m_videoHeight;
    double m_frameRate;
    double m_duration;
    int m_audioSampleRate;
    int m_audioChannels;

    // Audio output format (for resampling)
    int m_audioOutSampleRate;
    int m_audioOutChannels;

    // Thread management
    std::thread m_decodeThread;
    std::atomic<bool> m_running;
    std::atomic<PlayerState> m_state;
    std::atomic<double> m_currentTime;

    // Seek handling
    std::atomic<bool> m_seekRequested;
    std::atomic<double> m_seekTime;
    std::mutex m_seekMutex;

    // Pause handling
    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCondition;

    // Callbacks
    VideoFrameCallback m_videoCallback;
    VideoFrameCallback m_vcamCallback;
    AudioFrameCallback m_audioCallback;

    // Error handling
    std::string m_lastError;

    // Timing
    double m_videoTimeBase;
    double m_audioTimeBase;
};

} // namespace OnlyAir
