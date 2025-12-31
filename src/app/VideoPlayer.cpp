#include "VideoPlayer.h"
#include <chrono>
#include <cstring>

namespace OnlyAir {

VideoPlayer::VideoPlayer()
    : m_formatContext(nullptr)
    , m_videoCodecContext(nullptr)
    , m_audioCodecContext(nullptr)
    , m_swsContext(nullptr)
    , m_swrContext(nullptr)
    , m_videoStreamIndex(-1)
    , m_audioStreamIndex(-1)
    , m_videoFrame(nullptr)
    , m_audioFrame(nullptr)
    , m_rgbFrame(nullptr)
    , m_packet(nullptr)
    , m_rgbBuffer(nullptr)
    , m_rgbBufferSize(0)
    , m_previewFilterGraph(nullptr)
    , m_previewBuffersrcCtx(nullptr)
    , m_previewBuffersinkCtx(nullptr)
    , m_previewFrame(nullptr)
    , m_vcamFilterGraph(nullptr)
    , m_vcamBuffersrcCtx(nullptr)
    , m_vcamBuffersinkCtx(nullptr)
    , m_vcamFrame(nullptr)
    , m_audioBuffer(nullptr)
    , m_audioBufferSize(0)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_frameRate(30.0)
    , m_duration(0.0)
    , m_audioSampleRate(44100)
    , m_audioChannels(2)
    , m_audioOutSampleRate(0)
    , m_audioOutChannels(0)
    , m_running(false)
    , m_state(PlayerState::Idle)
    , m_currentTime(0.0)
    , m_seekRequested(false)
    , m_seekTime(0.0)
    , m_videoTimeBase(0.0)
    , m_audioTimeBase(0.0)
{
}

VideoPlayer::~VideoPlayer()
{
    close();
}

bool VideoPlayer::open(const std::string& filePath)
{
    close();

    m_state = PlayerState::Loading;
    m_lastError.clear();

    // Open input file
    int ret = avformat_open_input(&m_formatContext, filePath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        m_lastError = "Failed to open file: " + std::string(errBuf);
        m_state = PlayerState::Error;
        return false;
    }

    // Find stream info
    ret = avformat_find_stream_info(m_formatContext, nullptr);
    if (ret < 0) {
        m_lastError = "Failed to find stream info";
        close();
        return false;
    }

    // Find video stream
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    // Find audio stream
    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex < 0 && m_audioStreamIndex < 0) {
        m_lastError = "No video or audio stream found";
        close();
        return false;
    }

    // Setup video decoder
    if (m_videoStreamIndex >= 0) {
        AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
        const AVCodec* videoCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);

        if (!videoCodec) {
            m_lastError = "Video codec not found";
            close();
            return false;
        }

        m_videoCodecContext = avcodec_alloc_context3(videoCodec);
        avcodec_parameters_to_context(m_videoCodecContext, videoStream->codecpar);

        ret = avcodec_open2(m_videoCodecContext, videoCodec, nullptr);
        if (ret < 0) {
            m_lastError = "Failed to open video codec";
            close();
            return false;
        }

        int srcWidth = m_videoCodecContext->width;
        int srcHeight = m_videoCodecContext->height;
        m_videoTimeBase = av_q2d(videoStream->time_base);

        // Calculate frame rate
        if (videoStream->avg_frame_rate.num > 0) {
            m_frameRate = av_q2d(videoStream->avg_frame_rate);
        } else if (videoStream->r_frame_rate.num > 0) {
            m_frameRate = av_q2d(videoStream->r_frame_rate);
        }

        // Get rotation from stream side data or metadata
        double rotation = 0.0;

        // Try to get rotation from display matrix (side data)
        const AVPacketSideData* sideData = nullptr;
        for (int i = 0; i < videoStream->codecpar->nb_coded_side_data; i++) {
            if (videoStream->codecpar->coded_side_data[i].type == AV_PKT_DATA_DISPLAYMATRIX) {
                sideData = &videoStream->codecpar->coded_side_data[i];
                break;
            }
        }
        if (sideData) {
            rotation = -av_display_rotation_get((int32_t*)sideData->data);
        }

        // Fallback: try metadata "rotate" tag
        if (rotation == 0.0) {
            AVDictionaryEntry* rotateTag = av_dict_get(videoStream->metadata, "rotate", nullptr, 0);
            if (rotateTag) {
                rotation = atof(rotateTag->value);
            }
        }

        // Normalize rotation to 0-360
        while (rotation < 0) rotation += 360;
        while (rotation >= 360) rotation -= 360;

        // Build transpose filter string based on rotation
        std::string transposeFilter;
        bool swapDimensions = false;

        if (rotation >= 85 && rotation <= 95) {
            // 90 degrees clockwise
            transposeFilter = "transpose=1,";
            swapDimensions = true;
        } else if (rotation >= 175 && rotation <= 185) {
            // 180 degrees
            transposeFilter = "transpose=1,transpose=1,";
        } else if (rotation >= 265 && rotation <= 275) {
            // 270 degrees clockwise (90 counter-clockwise)
            transposeFilter = "transpose=2,";
            swapDimensions = true;
        }
        // else: 0 degrees or close, no transpose needed

        // Set output dimensions (after rotation)
        if (swapDimensions) {
            m_videoWidth = srcHeight;
            m_videoHeight = srcWidth;
        } else {
            m_videoWidth = srcWidth;
            m_videoHeight = srcHeight;
        }

        // Buffer source args (using SOURCE dimensions, before transpose)
        char bufferSrcArgs[512];
        snprintf(bufferSrcArgs, sizeof(bufferSrcArgs),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            srcWidth, srcHeight, m_videoCodecContext->pix_fmt,
            videoStream->time_base.num, videoStream->time_base.den,
            m_videoCodecContext->sample_aspect_ratio.num,
            m_videoCodecContext->sample_aspect_ratio.den > 0 ? m_videoCodecContext->sample_aspect_ratio.den : 1);

        // Create Preview filter graph: transpose (if needed) + format=rgb24
        m_previewFilterGraph = avfilter_graph_alloc();
        if (m_previewFilterGraph) {
            const AVFilter* buffersrc = avfilter_get_by_name("buffer");
            const AVFilter* buffersink = avfilter_get_by_name("buffersink");

            avfilter_graph_create_filter(&m_previewBuffersrcCtx, buffersrc, "in", bufferSrcArgs, nullptr, m_previewFilterGraph);
            avfilter_graph_create_filter(&m_previewBuffersinkCtx, buffersink, "out", nullptr, nullptr, m_previewFilterGraph);

            enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE };
            av_opt_set_int_list(m_previewBuffersinkCtx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

            AVFilterInOut* outputs = avfilter_inout_alloc();
            AVFilterInOut* inputs = avfilter_inout_alloc();

            outputs->name = av_strdup("in");
            outputs->filter_ctx = m_previewBuffersrcCtx;
            outputs->pad_idx = 0;
            outputs->next = nullptr;

            inputs->name = av_strdup("out");
            inputs->filter_ctx = m_previewBuffersinkCtx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            // Preview filter: transpose + format=rgb24
            std::string previewFilterStr = transposeFilter + "format=rgb24";
            if (avfilter_graph_parse_ptr(m_previewFilterGraph, previewFilterStr.c_str(), &inputs, &outputs, nullptr) >= 0) {
                avfilter_graph_config(m_previewFilterGraph, nullptr);
            }

            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);

            m_previewFrame = av_frame_alloc();
        }

        // Create VCam filter graph: transpose (if needed) + hflip + format=rgb24
        m_vcamFilterGraph = avfilter_graph_alloc();
        if (m_vcamFilterGraph) {
            const AVFilter* buffersrc = avfilter_get_by_name("buffer");
            const AVFilter* buffersink = avfilter_get_by_name("buffersink");

            avfilter_graph_create_filter(&m_vcamBuffersrcCtx, buffersrc, "in", bufferSrcArgs, nullptr, m_vcamFilterGraph);
            avfilter_graph_create_filter(&m_vcamBuffersinkCtx, buffersink, "out", nullptr, nullptr, m_vcamFilterGraph);

            enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE };
            av_opt_set_int_list(m_vcamBuffersinkCtx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

            AVFilterInOut* outputs = avfilter_inout_alloc();
            AVFilterInOut* inputs = avfilter_inout_alloc();

            outputs->name = av_strdup("in");
            outputs->filter_ctx = m_vcamBuffersrcCtx;
            outputs->pad_idx = 0;
            outputs->next = nullptr;

            inputs->name = av_strdup("out");
            inputs->filter_ctx = m_vcamBuffersinkCtx;
            inputs->pad_idx = 0;
            inputs->next = nullptr;

            // VCam filter: transpose + format=rgb24 (same as preview, no hflip)
            std::string vcamFilterStr = transposeFilter + "format=rgb24";
            if (avfilter_graph_parse_ptr(m_vcamFilterGraph, vcamFilterStr.c_str(), &inputs, &outputs, nullptr) >= 0) {
                avfilter_graph_config(m_vcamFilterGraph, nullptr);
            }

            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);

            m_vcamFrame = av_frame_alloc();
        }
    }

    // Setup audio decoder
    if (m_audioStreamIndex >= 0) {
        AVStream* audioStream = m_formatContext->streams[m_audioStreamIndex];
        const AVCodec* audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);

        if (!audioCodec) {
            m_lastError = "Audio codec not found";
            // Continue without audio
            m_audioStreamIndex = -1;
        } else {
            m_audioCodecContext = avcodec_alloc_context3(audioCodec);
            avcodec_parameters_to_context(m_audioCodecContext, audioStream->codecpar);

            ret = avcodec_open2(m_audioCodecContext, audioCodec, nullptr);
            if (ret < 0) {
                m_audioStreamIndex = -1;
            } else {
                m_audioSampleRate = m_audioCodecContext->sample_rate;
                m_audioChannels = m_audioCodecContext->ch_layout.nb_channels;
                m_audioTimeBase = av_q2d(audioStream->time_base);

                // Use output format if set, otherwise use source format
                int outRate = (m_audioOutSampleRate > 0) ? m_audioOutSampleRate : m_audioSampleRate;
                int outChannels = (m_audioOutChannels > 0) ? m_audioOutChannels : m_audioChannels;

                // Setup resampler for float output at target sample rate
                m_swrContext = swr_alloc();
                AVChannelLayout outLayout;
                av_channel_layout_default(&outLayout, outChannels);

                swr_alloc_set_opts2(&m_swrContext,
                    &outLayout, AV_SAMPLE_FMT_FLT, outRate,
                    &m_audioCodecContext->ch_layout, m_audioCodecContext->sample_fmt, m_audioSampleRate,
                    0, nullptr);

                swr_init(m_swrContext);

                // Update output values for callback
                m_audioSampleRate = outRate;
                m_audioChannels = outChannels;

                // Allocate audio buffer
                m_audioBufferSize = 192000;  // Large enough for most cases
                m_audioBuffer = (float*)av_malloc(m_audioBufferSize * sizeof(float));
            }
        }
    }

    // Allocate frames and packet
    m_videoFrame = av_frame_alloc();
    m_audioFrame = av_frame_alloc();
    m_packet = av_packet_alloc();

    // Get duration
    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_duration = (double)m_formatContext->duration / AV_TIME_BASE;
    }

    m_state = PlayerState::Stopped;
    m_currentTime = 0.0;

    return true;
}

void VideoPlayer::close()
{
    stop();

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    if (m_swrContext) {
        swr_free(&m_swrContext);
    }

    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }

    // Free preview filter graph
    if (m_previewFrame) {
        av_frame_free(&m_previewFrame);
    }

    if (m_previewFilterGraph) {
        avfilter_graph_free(&m_previewFilterGraph);
        m_previewBuffersrcCtx = nullptr;
        m_previewBuffersinkCtx = nullptr;
    }

    // Free VCam filter graph
    if (m_vcamFrame) {
        av_frame_free(&m_vcamFrame);
    }

    if (m_vcamFilterGraph) {
        avfilter_graph_free(&m_vcamFilterGraph);
        m_vcamBuffersrcCtx = nullptr;
        m_vcamBuffersinkCtx = nullptr;
    }

    if (m_audioBuffer) {
        av_free(m_audioBuffer);
        m_audioBuffer = nullptr;
    }

    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
    }

    if (m_videoFrame) {
        av_frame_free(&m_videoFrame);
    }

    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }

    if (m_packet) {
        av_packet_free(&m_packet);
    }

    if (m_videoCodecContext) {
        avcodec_free_context(&m_videoCodecContext);
    }

    if (m_audioCodecContext) {
        avcodec_free_context(&m_audioCodecContext);
    }

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_duration = 0.0;
    m_state = PlayerState::Idle;
    m_currentTime = 0.0;
}

void VideoPlayer::play()
{
    if (!isLoaded()) return;

    PlayerState expected = PlayerState::Stopped;
    if (m_state.compare_exchange_strong(expected, PlayerState::Playing)) {
        // Join any existing thread before creating new one
        if (m_decodeThread.joinable()) {
            m_decodeThread.join();
        }
        m_running = true;
        m_decodeThread = std::thread(&VideoPlayer::decodeThread, this);
        return;
    }

    expected = PlayerState::Paused;
    if (m_state.compare_exchange_strong(expected, PlayerState::Playing)) {
        m_pauseCondition.notify_all();
        return;
    }

    expected = PlayerState::EndOfFile;
    if (m_state.compare_exchange_strong(expected, PlayerState::Playing)) {
        // Join the finished thread before creating new one
        if (m_decodeThread.joinable()) {
            m_decodeThread.join();
        }
        seek(0.0);
        m_running = true;
        m_decodeThread = std::thread(&VideoPlayer::decodeThread, this);
    }
}

void VideoPlayer::pause()
{
    PlayerState expected = PlayerState::Playing;
    m_state.compare_exchange_strong(expected, PlayerState::Paused);
}

void VideoPlayer::stop()
{
    m_running = false;
    m_state = PlayerState::Stopped;

    // Wake up paused thread
    m_pauseCondition.notify_all();

    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }

    // Reset to beginning
    if (m_formatContext) {
        av_seek_frame(m_formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
        if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
        if (m_audioCodecContext) avcodec_flush_buffers(m_audioCodecContext);
    }

    m_currentTime = 0.0;
}

void VideoPlayer::seek(double timeSeconds)
{
    if (!isLoaded()) return;

    std::lock_guard<std::mutex> lock(m_seekMutex);
    m_seekTime = timeSeconds;
    m_seekRequested = true;
}

void VideoPlayer::decodeThread()
{
    using namespace std::chrono;

    auto startTime = high_resolution_clock::now();
    double playbackStartPts = 0.0;
    bool firstFrame = true;

    while (m_running.load()) {
        // Check for pause
        if (m_state.load() == PlayerState::Paused) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseCondition.wait(lock, [this]() {
                return m_state.load() != PlayerState::Paused || !m_running.load();
            });
            startTime = high_resolution_clock::now();
            playbackStartPts = m_currentTime.load();
            continue;
        }

        // Handle seek
        if (m_seekRequested.load()) {
            std::lock_guard<std::mutex> lock(m_seekMutex);
            int64_t seekTarget = (int64_t)(m_seekTime.load() * AV_TIME_BASE);
            av_seek_frame(m_formatContext, -1, seekTarget, AVSEEK_FLAG_BACKWARD);

            if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
            if (m_audioCodecContext) avcodec_flush_buffers(m_audioCodecContext);

            m_currentTime = m_seekTime.load();
            playbackStartPts = m_currentTime.load();
            startTime = high_resolution_clock::now();
            m_seekRequested = false;
            firstFrame = true;
        }

        // Read packet
        int ret = av_read_frame(m_formatContext, m_packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                m_state = PlayerState::EndOfFile;
            }
            break;
        }

        // Decode video
        if (m_packet->stream_index == m_videoStreamIndex && (m_videoCallback || m_vcamCallback)) {
            ret = avcodec_send_packet(m_videoCodecContext, m_packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(m_videoCodecContext, m_videoFrame) >= 0) {
                    // Calculate PTS
                    double pts = 0.0;
                    if (m_videoFrame->pts != AV_NOPTS_VALUE) {
                        pts = m_videoFrame->pts * m_videoTimeBase;
                    }

                    if (firstFrame) {
                        playbackStartPts = pts;
                        startTime = high_resolution_clock::now();
                        firstFrame = false;
                    }

                    // Wait for correct presentation time
                    auto now = high_resolution_clock::now();
                    double elapsed = duration<double>(now - startTime).count();
                    double targetTime = pts - playbackStartPts;

                    if (targetTime > elapsed) {
                        double waitTime = targetTime - elapsed;
                        if (waitTime > 0 && waitTime < 1.0) {
                            std::this_thread::sleep_for(duration<double>(waitTime));
                        }
                    }

                    // Process through preview filter graph (transpose + RGB24)
                    if (m_videoCallback && m_previewBuffersrcCtx && m_previewBuffersinkCtx) {
                        if (av_buffersrc_add_frame_flags(m_previewBuffersrcCtx, m_videoFrame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                            while (av_buffersink_get_frame(m_previewBuffersinkCtx, m_previewFrame) >= 0) {
                                m_videoCallback(m_previewFrame->data[0], m_previewFrame->width, m_previewFrame->height,
                                               m_previewFrame->linesize[0], pts);
                                av_frame_unref(m_previewFrame);
                            }
                        }
                    }

                    // Process through VCam filter graph (transpose + hflip + RGB24)
                    if (m_vcamCallback && m_vcamBuffersrcCtx && m_vcamBuffersinkCtx) {
                        if (av_buffersrc_add_frame_flags(m_vcamBuffersrcCtx, m_videoFrame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                            while (av_buffersink_get_frame(m_vcamBuffersinkCtx, m_vcamFrame) >= 0) {
                                m_vcamCallback(m_vcamFrame->data[0], m_vcamFrame->width, m_vcamFrame->height,
                                               m_vcamFrame->linesize[0], pts);
                                av_frame_unref(m_vcamFrame);
                            }
                        }
                    }

                    m_currentTime = pts;
                }
            }
        }

        // Decode audio
        if (m_packet->stream_index == m_audioStreamIndex && m_audioCallback && m_swrContext) {
            ret = avcodec_send_packet(m_audioCodecContext, m_packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(m_audioCodecContext, m_audioFrame) >= 0) {
                    // Resample to float - use proper pointer handling
                    uint8_t* outBuffer = (uint8_t*)m_audioBuffer;
                    int maxOutSamples = m_audioBufferSize / m_audioChannels;

                    int outSamples = swr_convert(m_swrContext,
                                                  &outBuffer, maxOutSamples,
                                                  (const uint8_t**)m_audioFrame->extended_data,
                                                  m_audioFrame->nb_samples);

                    if (outSamples > 0) {
                        m_audioCallback(m_audioBuffer, outSamples, m_audioChannels, m_audioSampleRate);
                    }
                }
            }
        }

        av_packet_unref(m_packet);
    }

    m_running = false;
}

void VideoPlayer::setAudioOutputFormat(int sampleRate, int channels)
{
    m_audioOutSampleRate = sampleRate;
    m_audioOutChannels = channels;
}

void VideoPlayer::decodeFirstFrame()
{
    if (!isLoaded() || m_videoStreamIndex < 0) return;

    // Seek to beginning
    av_seek_frame(m_formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);

    // Read packets until we get a video frame
    while (true) {
        int ret = av_read_frame(m_formatContext, m_packet);
        if (ret < 0) break;

        if (m_packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_videoCodecContext, m_packet);
            if (ret >= 0) {
                if (avcodec_receive_frame(m_videoCodecContext, m_videoFrame) >= 0) {
                    // Process through VCam filter graph for thumbnail
                    if (m_vcamCallback && m_vcamBuffersrcCtx && m_vcamBuffersinkCtx) {
                        if (av_buffersrc_add_frame_flags(m_vcamBuffersrcCtx, m_videoFrame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                            if (av_buffersink_get_frame(m_vcamBuffersinkCtx, m_vcamFrame) >= 0) {
                                m_vcamCallback(m_vcamFrame->data[0], m_vcamFrame->width, m_vcamFrame->height,
                                               m_vcamFrame->linesize[0], 0.0);
                                av_frame_unref(m_vcamFrame);
                            }
                        }
                    }
                    av_packet_unref(m_packet);
                    break;
                }
            }
        }
        av_packet_unref(m_packet);
    }

    // Reset to beginning for later playback
    av_seek_frame(m_formatContext, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
}

} // namespace OnlyAir
