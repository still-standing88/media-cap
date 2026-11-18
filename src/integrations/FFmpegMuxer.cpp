#include "FFmpegMuxer.h"
#include "FFmpegPolicy.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cmath>
#include <climits>
#include "utils/RecordingTimeline.h"
#include "utils/Utf8.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/version.h>
}

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 24, 100)
#error "MediaCap requires FFmpeg 5.1 or newer (libavutil >= 57.24.100) for the AVChannelLayout API. Verified against FFmpeg 7.1.1."
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

namespace MediaCap {

    static size_t frame_bytes(const PackedFrame& frame) {
        return std::visit([](const auto& f) -> size_t {
            using T = std::decay_t<decltype(f)>;
            if constexpr (std::is_same_v<T, MediaCap_AudioFrame>)
                return size_t(f.num_samples) * f.format.channels * 8;
            else
                return size_t(std::abs(f.linesize[0])) * f.height * (f.data[1] ? 2 : 1);
        }, frame);
    }

    struct EncoderChoice {
        std::vector<const char*> candidates;
        const char* feature_hint;
    };

    static const AVCodec* find_encoder(const EncoderChoice& choice, const char* what) {
        for (size_t i = 0; i < choice.candidates.size(); ++i) {
            if (const AVCodec* codec = avcodec_find_encoder_by_name(choice.candidates[i])) {
                if (i > 0) {
                    std::cerr << "MediaCap Warning: " << what << " encoder '" << choice.candidates[0]
                              << "' is not available in this FFmpeg build; using '" << choice.candidates[i]
                              << "' instead." << std::endl;
                }
                return codec;
            }
        }

        std::cerr << "MediaCap FFmpeg Error: no usable " << what << " encoder in this FFmpeg build. Tried:";
        for (const char* name : choice.candidates) {
            std::cerr << " " << name;
        }
        std::cerr << ".\n  This is a build-configuration problem, not a runtime one: rebuild FFmpeg with the '"
                  << choice.feature_hint << "' feature enabled (vcpkg: `vcpkg install \"ffmpeg["
                  << choice.feature_hint << ",...]:" << "<triplet>\" --recurse`)." << std::endl;
        return nullptr;
    }

    void FFmpegMuxer::fail(MediaCap_ErrorCode code, const char* message, int64_t native) {
        if (!m_failed.exchange(true)) errors.report(code, message, native);
    }
    void FFmpegMuxer::log_ffmpeg_error(int err, const std::string& message) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
        av_strerror(err, err_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "MediaCap FFmpeg Error: " << message << " (" << err_buf << ")" << std::endl;
        const bool io = err == AVERROR(EIO) || err == AVERROR(EACCES) || err == AVERROR(ENOENT) || err == AVERROR(ENOSPC);
        fail(io ? MEDIACAP_ERROR_IO : MEDIACAP_ERROR_ENCODING_FAILED, message.c_str(), err);
    }

    FFmpegMuxer::StreamContext::~StreamContext() {
        avcodec_free_context(&codec_context);
        av_frame_free(&frame);
        av_frame_free(&resampled_frame);
        av_frame_free(&scratch);
        av_packet_free(&packet);
        sws_freeContext(sws_context);
        swr_free(&swr_context);
        av_audio_fifo_free(audio_fifo);
        audio_fifo = nullptr;
    }

    FFmpegMuxer::FFmpegMuxer(const MediaCap_RecordingConfig& config) {
        m_config_storage = config;
        m_output_filename = config.output_filename ? config.output_filename : "";
        m_config_storage.output_filename = m_output_filename.c_str();
        if (config.video_config) {
            m_video_config_storage = *config.video_config;
            m_config_storage.video_config = &m_video_config_storage;
        }
        if (config.audio_config) {
            m_audio_config_storage = *config.audio_config;
            m_config_storage.audio_config = &m_audio_config_storage;
        }
    }

    FFmpegMuxer::~FFmpegMuxer() {
        stop();
        cleanup();
    }

    bool FFmpegMuxer::start() {
        if (m_is_running.load()) return true;
        if (!initialize()) {
            fail(MEDIACAP_ERROR_ENCODING_FAILED, "Could not initialize the recording encoder/output");
            cleanup();
            return false;
        }
        m_is_running.store(true);
        m_muxer_thread = std::thread(&FFmpegMuxer::muxer_thread_func, this);
        return true;
    }

    void FFmpegMuxer::stop(int64_t end_ns) {
        {
            std::lock_guard<std::mutex> lock(m_admission_mutex);
            if (end_ns >= 0) m_end_timestamp_ns.store(end_ns);
            m_is_running.store(false);
        }
        if (m_muxer_thread.joinable()) m_muxer_thread.join();
    }

    bool FFmpegMuxer::add_frame(std::shared_ptr<PackedFrame> frame) {
        std::unique_lock<std::mutex> lock(m_admission_mutex);
        if (!m_is_running.load() || !frame || m_failed.load()) return false;
        const size_t bytes = frame_bytes(*frame);
        if (bytes > 128 * 1024 * 1024 || m_queued_bytes + bytes > 128 * 1024 * 1024 ||
            !m_frame_queue.try_push(std::move(frame), 2048)) {
            lock.unlock();
            fail(MEDIACAP_ERROR_BUFFER_OVERFLOW, "Encoder queue overflow; recording could not keep up");
            return false;
        }
        m_queued_bytes += bytes;
        return true;
    }

    static const char* muxer_name(MediaCap_ContainerFormat container) {
        static constexpr const char* names[]{nullptr, "mp4", "matroska", "webm", "ipod", "flac", "mp3", "wav", "opus"};
        const auto index = static_cast<unsigned>(container);
        return index < sizeof(names) / sizeof(*names) ? names[index] : nullptr;
    }

    static MediaCap_VideoCodec default_video(const AVOutputFormat& format) {
        return std::string_view(format.name) == "webm" ? MEDIACAP_VIDEO_CODEC_VP9 : MEDIACAP_VIDEO_CODEC_H264;
    }

    static MediaCap_AudioCodec default_audio(const AVOutputFormat& format) {
        switch (format.audio_codec) {
        case AV_CODEC_ID_OPUS: return MEDIACAP_AUDIO_CODEC_OPUS;
        case AV_CODEC_ID_VORBIS: return MEDIACAP_AUDIO_CODEC_VORBIS;
        case AV_CODEC_ID_FLAC: return MEDIACAP_AUDIO_CODEC_FLAC;
        case AV_CODEC_ID_MP3: return MEDIACAP_AUDIO_CODEC_MP3;
        case AV_CODEC_ID_PCM_S16LE: return MEDIACAP_AUDIO_CODEC_PCM;
        default: return MEDIACAP_AUDIO_CODEC_AAC;
        }
    }

    bool FFmpegMuxer::validate_format(const MediaCap_RecordingConfig& config, const Errors::Reporter& reporter) {
        const auto reject = [&](MediaCap_ErrorCode code, const std::string& message) {
            reporter.report(code, message.c_str());
            return false;
        };
        if (!config.output_filename || !*config.output_filename || !Utf8::valid(config.output_filename))
            return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Recording output requires a nonempty UTF-8 file path");
        if (!config.video_config && !config.audio_config)
            return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Recording requires an audio or video encoder configuration");
        const auto* name = muxer_name(config.container);
        if (!name && config.container != MEDIACAP_CONTAINER_DEFAULT)
            return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Unknown output container");
        const auto* format = av_guess_format(name, config.output_filename, nullptr);
        if (!format)
            return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Cannot infer an output container from the filename. Use a supported extension or --container");

        const std::string label = std::string_view(format->name) == "ipod" ? "m4a" : format->name;
        if (config.video_config) {
            if (format->video_codec == AV_CODEC_ID_NONE || label == "m4a" || label == "ogg" ||
                label == "flac" || label == "mp3" || label == "wav" || label == "opus")
                return reject(MEDIACAP_ERROR_UNSUPPORTED, "Video sources cannot be recorded in the " + label + " audio-only container");
            const auto selected = config.video_config->codec == MEDIACAP_VIDEO_CODEC_DEFAULT
                ? default_video(*format) : config.video_config->codec;
            AVCodecID codec;
            switch (selected) {
            case MEDIACAP_VIDEO_CODEC_H264: codec = AV_CODEC_ID_H264; break;
            case MEDIACAP_VIDEO_CODEC_VP9: codec = AV_CODEC_ID_VP9; break;
            case MEDIACAP_VIDEO_CODEC_AV1: codec = AV_CODEC_ID_AV1; break;
            default: return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Unknown video codec");
            }
            if ((label == "webm" && codec == AV_CODEC_ID_H264) ||
                avformat_query_codec(format, codec, FF_COMPLIANCE_NORMAL) == 0)
                return reject(MEDIACAP_ERROR_UNSUPPORTED, std::string("Video codec ") + avcodec_get_name(codec) +
                    " is incompatible with the " + label + " container");
        }
        if (config.audio_config) {
            const auto& audio = *config.audio_config;
            const auto selected = audio.codec == MEDIACAP_AUDIO_CODEC_DEFAULT ? default_audio(*format) : audio.codec;
            AVCodecID codec;
            const char* encoder;
            switch (selected) {
            case MEDIACAP_AUDIO_CODEC_AAC: codec = AV_CODEC_ID_AAC; encoder = "aac"; break;
            case MEDIACAP_AUDIO_CODEC_OPUS: codec = AV_CODEC_ID_OPUS; encoder = "libopus"; break;
            case MEDIACAP_AUDIO_CODEC_VORBIS: codec = AV_CODEC_ID_VORBIS; encoder = "libvorbis"; break;
            case MEDIACAP_AUDIO_CODEC_FLAC: codec = AV_CODEC_ID_FLAC; encoder = "flac"; break;
            case MEDIACAP_AUDIO_CODEC_MP3: codec = AV_CODEC_ID_MP3; encoder = "libmp3lame"; break;
            case MEDIACAP_AUDIO_CODEC_PCM: codec = AV_CODEC_ID_PCM_S16LE; encoder = "pcm_s16le"; break;
            default: return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Unknown audio codec");
            }
            if ((label == "m4a" && codec != AV_CODEC_ID_AAC) ||
                (label == "flac" && codec != AV_CODEC_ID_FLAC) ||
                (label == "mp3" && codec != AV_CODEC_ID_MP3) ||
                (label == "opus" && codec != AV_CODEC_ID_OPUS) ||
                (label == "webm" && codec != AV_CODEC_ID_OPUS && codec != AV_CODEC_ID_VORBIS) ||
                (label == "ogg" && codec != AV_CODEC_ID_VORBIS && codec != AV_CODEC_ID_OPUS && codec != AV_CODEC_ID_FLAC) ||
                avformat_query_codec(format, codec, FF_COMPLIANCE_NORMAL) == 0)
                return reject(MEDIACAP_ERROR_UNSUPPORTED, std::string("Audio codec ") + avcodec_get_name(codec) +
                    " is incompatible with the " + label + " container");
            if (!audio.sample_rate || audio.sample_rate > INT_MAX || !audio.channels)
                return reject(MEDIACAP_ERROR_INVALID_ARGUMENT, "Audio output requires a valid sample rate and channel count");
            if (const auto* implementation = avcodec_find_encoder_by_name(encoder)) {
                if (const auto* rate = implementation->supported_samplerates) {
                    while (*rate && *rate != static_cast<int>(audio.sample_rate)) ++rate;
                    if (!*rate)
                        return reject(MEDIACAP_ERROR_UNSUPPORTED, std::string("Audio codec ") + avcodec_get_name(codec) +
                            " does not support the requested sample rate of " + std::to_string(audio.sample_rate) + " Hz");
                }
                if (const auto* layout = implementation->ch_layouts) {
                    AVChannelLayout requested{};
                    av_channel_layout_default(&requested, audio.channels);
                    while (layout->nb_channels && av_channel_layout_compare(layout, &requested) != 0) ++layout;
                    av_channel_layout_uninit(&requested);
                    if (!layout->nb_channels)
                        return reject(MEDIACAP_ERROR_UNSUPPORTED, std::string("Audio codec ") + avcodec_get_name(codec) +
                            " does not support the requested " + std::to_string(audio.channels) + "-channel layout");
                }
            }
        }
        return true;
    }

    void FFmpegMuxer::set_default_codecs() {
        if (m_config_storage.video_config && m_config_storage.video_config->codec == MEDIACAP_VIDEO_CODEC_DEFAULT) {
            m_config_storage.video_config->codec = default_video(*m_format_context->oformat);
        }
        if (m_config_storage.audio_config && m_config_storage.audio_config->codec == MEDIACAP_AUDIO_CODEC_DEFAULT) {
            m_config_storage.audio_config->codec = default_audio(*m_format_context->oformat);
        }
    }

    bool FFmpegMuxer::initialize() {
        if (!FFmpegPolicy::compatible()) {
            errors.report(MEDIACAP_ERROR_UNSUPPORTED, "GPL/nonfree or unknown FFmpeg license rejected");
            return false;
        }
        if (!validate_format(m_config_storage, errors)) {
            m_failed.store(true);
            return false;
        }

        if (m_output_filename.empty() ||
            (m_config_storage.video_config && (m_config_storage.video_config->framerate <= 0 ||
                m_config_storage.video_config->width < 2 || m_config_storage.video_config->height < 2)) ||
            (m_config_storage.audio_config && (!m_config_storage.audio_config->sample_rate ||
                !m_config_storage.audio_config->channels))) {
            fail(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid output path, dimensions, frame rate, or audio format"); return false;
        }
        int ret = avformat_alloc_output_context2(&m_format_context, nullptr,
            muxer_name(m_config_storage.container), m_config_storage.output_filename);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not allocate output context");
            return false;
        }
        set_default_codecs();

        if (m_config_storage.video_config && !initialize_video_stream()) return false;
        if (m_config_storage.audio_config && !initialize_audio_stream()) return false;

        av_dump_format(m_format_context, 0, m_config_storage.output_filename, 1);

        if (!(m_format_context->oformat->flags & AVFMT_NOFILE)) {
            const auto file_url = "file:" + m_output_filename;
            ret = avio_open(&m_format_context->pb, file_url.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                log_ffmpeg_error(ret, "Could not open output file");
                return false;
            }
        }
        ret = avformat_write_header(m_format_context, nullptr);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not write output container header");
            return false;
        }
        m_header_written = true;
        return true;
    }

    bool FFmpegMuxer::initialize_video_stream() {
        const AVCodec* codec;
        switch (m_config_storage.video_config->codec) {
        case MEDIACAP_VIDEO_CODEC_H264:
            codec = find_encoder({ { "libopenh264", "h264_mf", "h264_nvenc", "h264_qsv", "h264_amf" },
                                   "openh264" }, "H.264 video");
            break;
        case MEDIACAP_VIDEO_CODEC_VP9:
            codec = find_encoder({ { "libvpx-vp9", "vp9_qsv" }, "vpx" }, "VP9 video");
            break;
        case MEDIACAP_VIDEO_CODEC_AV1:
            codec = find_encoder({ { "libaom-av1", "libsvtav1", "librav1e", "av1_nvenc", "av1_qsv" }, "aom" },
                                 "AV1 video");
            break;
        default: std::cerr << "Unsupported video codec" << std::endl; return false;
        }
        if (!codec) {
            return false;
        }

        m_video_stream.stream = avformat_new_stream(m_format_context, nullptr);
        if (!m_video_stream.stream) {
            std::cerr << "MediaCap FFmpeg Error: Failed allocating video stream" << std::endl;
            return false;
        }

        m_video_stream.codec_context = avcodec_alloc_context3(codec);
        if (!m_video_stream.codec_context) {
            std::cerr << "MediaCap FFmpeg Error: Failed to allocate video codec context" << std::endl;
            return false;
        }

        auto* c = m_video_stream.codec_context;
        c->bit_rate = m_config_storage.video_config->bitrate_bps;

        c->width = m_config_storage.video_config->width & ~1;
        c->height = m_config_storage.video_config->height & ~1;

        c->time_base = { 1, m_config_storage.video_config->framerate };
        c->framerate = { m_config_storage.video_config->framerate, 1 };
        m_video_stream.stream->avg_frame_rate = c->framerate;
        c->gop_size = 12;
        c->pix_fmt = AV_PIX_FMT_YUV420P;
        if (m_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
            c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        if (std::string(codec->name) == "libopenh264") {
            c->thread_count = 0;
            c->slices = std::max(1u, std::min(std::thread::hardware_concurrency(),
                unsigned((c->height + 15) / 16)));

        }

        int ret = avcodec_open2(c, codec, nullptr);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not open video codec");
            return false;
        }

        ret = avcodec_parameters_from_context(m_video_stream.stream->codecpar, c);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not copy video stream parameters");
            return false;
        }

        m_video_stream.stream->time_base = c->time_base;
        m_video_stream.frame = av_frame_alloc();
        m_video_stream.frame->format = c->pix_fmt;
        m_video_stream.frame->width = c->width;
        m_video_stream.frame->height = c->height;
        if (av_frame_get_buffer(m_video_stream.frame, 0) < 0) {
            std::cerr << "Could not allocate video frame data" << std::endl;
            return false;
        }

        m_video_stream.packet = av_packet_alloc();
        return true;
    }

    bool FFmpegMuxer::initialize_audio_stream() {
        const AVCodec* codec;
        switch (m_config_storage.audio_config->codec) {
        case MEDIACAP_AUDIO_CODEC_AAC:
            codec = find_encoder({ { "aac", "aac_mf" }, "avcodec" }, "AAC audio");
            break;
        case MEDIACAP_AUDIO_CODEC_OPUS:
            codec = find_encoder({ { "libopus", "opus" }, "opus" }, "Opus audio");
            break;
        case MEDIACAP_AUDIO_CODEC_VORBIS:
            codec = find_encoder({ { "libvorbis", "vorbis" }, "vorbis" }, "Vorbis audio");
            break;
        case MEDIACAP_AUDIO_CODEC_FLAC:
            codec = find_encoder({ { "flac" }, "avcodec" }, "FLAC audio");
            break;
        case MEDIACAP_AUDIO_CODEC_MP3:
            codec = find_encoder({ { "libmp3lame", "libshine", "mp3_mf" }, "mp3lame" }, "MP3 audio");
            break;
        case MEDIACAP_AUDIO_CODEC_PCM:
            codec = find_encoder({ { "pcm_s16le" }, "avcodec" }, "PCM audio");
            break;
        default: std::cerr << "Unsupported audio codec" << std::endl; return false;
        }
        if (!codec) {
            return false;
        }

        m_audio_stream.stream = avformat_new_stream(m_format_context, nullptr);
        if (!m_audio_stream.stream) {
            std::cerr << "MediaCap FFmpeg Error: Failed allocating audio stream" << std::endl;
            return false;
        }

        m_audio_stream.codec_context = avcodec_alloc_context3(codec);
        if (!m_audio_stream.codec_context) {
            std::cerr << "MediaCap FFmpeg Error: Failed to allocate audio codec context" << std::endl;
            return false;
        }

        auto* c = m_audio_stream.codec_context;
        c->bit_rate = m_config_storage.audio_config->bitrate_bps;
        c->sample_rate = m_config_storage.audio_config->sample_rate;
        c->time_base = {1, c->sample_rate};

        AVChannelLayout ch_layout = {};
        av_channel_layout_default(&ch_layout, m_config_storage.audio_config->channels);
        av_channel_layout_copy(&c->ch_layout, &ch_layout);
        av_channel_layout_uninit(&ch_layout);

        c->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        m_audio_stream.stream->time_base = { 1, c->sample_rate };

        if (m_format_context->oformat->flags & AVFMT_GLOBALHEADER) {
            c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int ret = avcodec_open2(c, codec, nullptr);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not open audio codec");
            return false;
        }

        ret = avcodec_parameters_from_context(m_audio_stream.stream->codecpar, c);
        if (ret < 0) {
            log_ffmpeg_error(ret, "Could not copy audio stream parameters");
            return false;
        }

        m_audio_stream.resampled_frame = av_frame_alloc();
        m_audio_stream.resampled_frame->format = c->sample_fmt;
        av_channel_layout_copy(&m_audio_stream.resampled_frame->ch_layout, &c->ch_layout);
        m_audio_stream.resampled_frame->sample_rate = c->sample_rate;
        m_audio_stream.resampled_frame->nb_samples = c->frame_size > 0 ? c->frame_size : 1024;
        if (av_frame_get_buffer(m_audio_stream.resampled_frame, 0) < 0) {
            std::cerr << "Could not allocate audio data buffers" << std::endl;
            return false;
        }
        m_audio_stream.resampled_capacity = m_audio_stream.resampled_frame->nb_samples;

        m_audio_stream.last_pts = 0;
        m_audio_stream.audio_fifo = av_audio_fifo_alloc(c->sample_fmt, c->ch_layout.nb_channels, 1);
        if (!m_audio_stream.audio_fifo) return false;
        m_audio_stream.packet = av_packet_alloc();
        return true;
    }

    void FFmpegMuxer::muxer_thread_func() {
        try {
            while (m_is_running.load() || !m_frame_queue.empty()) {
                std::shared_ptr<PackedFrame> frame;
                if (m_frame_queue.wait_and_pop_for(frame, std::chrono::milliseconds(10))) {
                    {
                        std::lock_guard<std::mutex> lock(m_admission_mutex);
                        m_queued_bytes -= frame_bytes(*frame);
                    }
                    if (!process_frame(frame)) {
                        fail(MEDIACAP_ERROR_ENCODING_FAILED, "Encoding failed; output is incomplete");
                    }
                }
                if (m_is_running.load() && !m_failed.load() && m_media_clock && m_frame_queue.empty() &&
                    !advance_video(std::max<int64_t>(0, m_media_clock() - 40000000)))
                    fail(MEDIACAP_ERROR_ENCODING_FAILED, "Could not encode held video frame");
            }
            finalize();
        } catch (const std::exception& error) {
            fail(MEDIACAP_ERROR_ENCODING_FAILED, error.what());
        } catch (...) {
            fail(MEDIACAP_ERROR_INTERNAL, "Unexpected encoder worker exception");
        }
    }

    bool FFmpegMuxer::advance_video(int64_t end_ns) {
        if (!m_video_has_frame || end_ns < 0) return true;
        const int64_t ticks = av_rescale_q_rnd(end_ns, {1, 1000000000},
            m_video_stream.codec_context->time_base, AV_ROUND_UP);
        while (m_video_next_pts.load() < ticks) {
            m_video_stream.frame->pts = m_video_next_pts++;
            if (!write_frame(m_video_stream, m_video_stream.frame)) return false;
        }
        return true;
    }

    bool FFmpegMuxer::process_frame(const std::shared_ptr<PackedFrame>& frame_ptr) {
        if (!m_header_written) {
            std::lock_guard<std::mutex> lock(m_muxer_mutex);
            if (!m_header_written) {
                int ret = avformat_write_header(m_format_context, nullptr);
                if (ret < 0) {
                    log_ffmpeg_error(ret, "Error occurred when opening output file");
                    return false;
                }
                m_header_written = true;
            }
        }

        return std::visit([this](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, MediaCap_VideoFrame>)
                return !m_video_stream.codec_context || encode_video_frame(arg);
            else
                return !m_audio_stream.codec_context || encode_audio_frame(arg);
        }, *frame_ptr);
    }

    bool FFmpegMuxer::encode_video_frame(const MediaCap_VideoFrame& video_frame) {
        auto& ctx = m_video_stream;
        const int64_t tick = av_rescale_q_rnd(video_frame.timestamp_ns, {1, 1000000000},
            ctx.codec_context->time_base, AV_ROUND_DOWN);
        if (tick < 0 || (m_video_has_frame && tick < m_video_next_pts)) return true;
        if (m_video_has_frame) {
            while (m_video_next_pts < tick) {
                ctx.frame->pts = m_video_next_pts++;
                if (!write_frame(ctx, ctx.frame)) return false;
            }
        } else m_video_next_pts = tick;
        AVPixelFormat format;
        switch (video_frame.pixel_format) {
        case MEDIACAP_PIXEL_FORMAT_BGRA: format = AV_PIX_FMT_BGRA; break;
        case MEDIACAP_PIXEL_FORMAT_BGR24: format = AV_PIX_FMT_BGR24; break;
        case MEDIACAP_PIXEL_FORMAT_RGBA: format = AV_PIX_FMT_RGBA; break;
        case MEDIACAP_PIXEL_FORMAT_RGB24: format = AV_PIX_FMT_RGB24; break;
        case MEDIACAP_PIXEL_FORMAT_NV12: format = AV_PIX_FMT_NV12; break;
        case MEDIACAP_PIXEL_FORMAT_I420: format = AV_PIX_FMT_YUV420P; break;
        default: return false;
        }
        ctx.sws_context = sws_getCachedContext(ctx.sws_context, video_frame.width, video_frame.height, format,
            ctx.codec_context->width, ctx.codec_context->height, ctx.codec_context->pix_fmt,
            SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!ctx.sws_context || av_frame_make_writable(ctx.frame) < 0) return false;
        if (sws_scale(ctx.sws_context, video_frame.data, video_frame.linesize, 0, video_frame.height,
            ctx.frame->data, ctx.frame->linesize) <= 0) return false;
        ctx.frame->pts = m_video_next_pts++;
        m_video_has_frame = true;
        return write_frame(ctx, ctx.frame);
    }

    bool FFmpegMuxer::prepare_scratch(int capacity) {
        auto& ctx = m_audio_stream;
        if (ctx.scratch && ctx.scratch->nb_samples >= capacity) return av_frame_make_writable(ctx.scratch) >= 0;
        av_frame_free(&ctx.scratch);
        ctx.scratch = av_frame_alloc();
        if (!ctx.scratch) return false;
        ctx.scratch->format = ctx.codec_context->sample_fmt;
        ctx.scratch->sample_rate = ctx.codec_context->sample_rate;
        av_channel_layout_copy(&ctx.scratch->ch_layout, &ctx.codec_context->ch_layout);
        ctx.scratch->nb_samples = std::max(4096, capacity);
        return av_frame_get_buffer(ctx.scratch, 0) >= 0;
    }

    bool FFmpegMuxer::push_audio(uint8_t** data, int frames) {
        auto& ctx = m_audio_stream;
        if (frames <= 0) return true;
        if (av_audio_fifo_realloc(ctx.audio_fifo, av_audio_fifo_size(ctx.audio_fifo) + frames) < 0 ||
            av_audio_fifo_write(ctx.audio_fifo, reinterpret_cast<void**>(data), frames) != frames) return false;
        ctx.audio_tail += frames;
        return drain_audio_fifo();
    }

    bool FFmpegMuxer::fill_audio_gap(int64_t target) {
        auto& ctx = m_audio_stream;
        AVFrame* silence = av_frame_alloc();
        if (!silence) return false;
        silence->format = ctx.codec_context->sample_fmt;
        silence->sample_rate = ctx.codec_context->sample_rate;
        av_channel_layout_copy(&silence->ch_layout, &ctx.codec_context->ch_layout);
        silence->nb_samples = 4096;
        bool ok = av_frame_get_buffer(silence, 0) >= 0;
        if (ok) av_samples_set_silence(silence->extended_data, 0, 4096,
            ctx.codec_context->ch_layout.nb_channels, ctx.codec_context->sample_fmt);
        while (ok && ctx.audio_tail < target)
            ok = push_audio(silence->extended_data, int(std::min<int64_t>(4096, target - ctx.audio_tail)));
        av_frame_free(&silence);
        return ok;
    }

    bool FFmpegMuxer::append_audio(uint8_t** data, int frames, int64_t start) {
        auto& ctx = m_audio_stream;
        const int64_t end_ns = m_end_timestamp_ns.load();
        if (end_ns >= 0) frames = int(std::min<int64_t>(frames,
            std::max<int64_t>(0, ns_to_samples(end_ns, ctx.codec_context->sample_rate) - start)));
        if (frames <= 0) return true;
        if (start > ctx.audio_tail && !fill_audio_gap(start)) return false;
        const int skip = int(std::min<int64_t>(frames, std::max<int64_t>(0, ctx.audio_tail - start)));
        const bool planar = av_sample_fmt_is_planar(ctx.codec_context->sample_fmt) != 0;
        const int channels = ctx.codec_context->ch_layout.nb_channels;
        const int planes = planar ? channels : 1;
        const int stride = av_get_bytes_per_sample(ctx.codec_context->sample_fmt) * (planar ? 1 : channels);
        ctx.shifted_planes.resize(planes);
        for (int i = 0; i < planes; ++i) ctx.shifted_planes[i] = data[i] + size_t(skip) * stride;
        return push_audio(ctx.shifted_planes.data(), frames - skip);
    }

    bool FFmpegMuxer::flush_resampler() {
        auto& ctx = m_audio_stream;
        if (!ctx.swr_context) return true;
        for (;;) {
            const int capacity = std::max(256, swr_get_out_samples(ctx.swr_context, 0));
            if (!prepare_scratch(capacity)) return false;
            const int converted = swr_convert(ctx.swr_context, ctx.scratch->extended_data,
                capacity, nullptr, 0);
            if (converted < 0) return false;
            if (!converted) return true;
            if (!append_audio(ctx.scratch->extended_data, converted, ctx.converted_next)) return false;
            ctx.converted_next += converted;
        }
    }

    bool FFmpegMuxer::encode_audio_frame(const MediaCap_AudioFrame& frame) {
        auto& ctx = m_audio_stream;
        if (!frame.format.sample_rate || !frame.format.channels || !frame.data || !frame.data[0]) return false;
        AVSampleFormat format;
        switch (frame.format.format) {
        case MEDIACAP_SAMPLE_FORMAT_FLT: format = AV_SAMPLE_FMT_FLT; break;
        case MEDIACAP_SAMPLE_FORMAT_S16: format = AV_SAMPLE_FMT_S16; break;
        case MEDIACAP_SAMPLE_FORMAT_S32: format = AV_SAMPLE_FMT_S32; break;
        case MEDIACAP_SAMPLE_FORMAT_U8: format = AV_SAMPLE_FMT_U8; break;
        default: return false;
        }
        const int64_t expected = ctx.input_anchor_ns + (ctx.input_format.sample_rate ?
            samples_to_ns(ctx.input_samples, ctx.input_format.sample_rate) : 0);
        if (!ctx.swr_context || ctx.input_format.format != frame.format.format ||
            ctx.input_format.channels != frame.format.channels || ctx.input_format.sample_rate != frame.format.sample_rate ||
            std::abs(frame.timestamp_ns - expected) > samples_to_ns(2, frame.format.sample_rate)) {
            if (!flush_resampler()) return false;
            swr_free(&ctx.swr_context);
            AVChannelLayout layout{};
            av_channel_layout_default(&layout, frame.format.channels);
            const int result = swr_alloc_set_opts2(&ctx.swr_context, &ctx.codec_context->ch_layout,
                ctx.codec_context->sample_fmt, ctx.codec_context->sample_rate,
                &layout, format, frame.format.sample_rate, 0, nullptr);
            av_channel_layout_uninit(&layout);
            if (result < 0 || !ctx.swr_context || swr_init(ctx.swr_context) < 0) return false;
            ctx.input_format = frame.format; ctx.input_anchor_ns = frame.timestamp_ns; ctx.input_samples = 0;
            ctx.converted_next = av_rescale_q(frame.timestamp_ns, {1, 1000000000}, ctx.codec_context->time_base);
        }
        const int capacity = swr_get_out_samples(ctx.swr_context, int(frame.num_samples));
        if (capacity < 0 || !prepare_scratch(capacity)) return false;
        const uint8_t* input = frame.data[0];
        const int converted = swr_convert(ctx.swr_context, ctx.scratch->extended_data,
            capacity, &input, int(frame.num_samples));
        if (converted < 0 || !append_audio(ctx.scratch->extended_data, converted, ctx.converted_next)) return false;
        ctx.converted_next += converted; ctx.input_samples += frame.num_samples;
        return true;
    }

    bool FFmpegMuxer::write_frame(StreamContext& context, AVFrame* frame_to_encode) {
        std::lock_guard<std::mutex> lock(m_muxer_mutex);
        int ret = avcodec_send_frame(context.codec_context, frame_to_encode);
        if (ret < 0) {
            if (ret != AVERROR(EAGAIN)) {
                log_ffmpeg_error(ret, "Error sending frame to encoder");
            }
            return false;
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(context.codec_context, context.packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                return true;
            }
            else if (ret < 0) {
                log_ffmpeg_error(ret, "Error during encoding");
                return false;
            }

            if (context.codec_context->codec_type == AVMEDIA_TYPE_VIDEO && context.packet->duration <= 0)
                context.packet->duration = 1;
            av_packet_rescale_ts(context.packet, context.codec_context->time_base, context.stream->time_base);
            context.packet->stream_index = context.stream->index;

            ret = av_interleaved_write_frame(m_format_context, context.packet);
            if (ret < 0) {
                log_ffmpeg_error(ret, "Error while writing output packet");
                return false;
            }
        }
        return true;
    }

    bool FFmpegMuxer::drain_audio_fifo(bool final) {
        auto& ctx = m_audio_stream;
        if (!ctx.audio_fifo) return true;
        int available;
        while ((available = av_audio_fifo_size(ctx.audio_fifo)) > 0 &&
            (final || available >= ctx.resampled_capacity)) {
            const int count = std::min(available, ctx.resampled_capacity);
            ctx.resampled_frame->nb_samples = ctx.resampled_capacity;
            if (av_frame_make_writable(ctx.resampled_frame) < 0) return false;
            if (av_audio_fifo_read(ctx.audio_fifo, reinterpret_cast<void**>(ctx.resampled_frame->extended_data), count) != count)
                return false;
            int encoded = count;
            if (count < ctx.resampled_capacity &&
                !(ctx.codec_context->codec->capabilities & (AV_CODEC_CAP_SMALL_LAST_FRAME | AV_CODEC_CAP_VARIABLE_FRAME_SIZE))) {
                encoded = ctx.resampled_capacity;
                av_samples_set_silence(ctx.resampled_frame->extended_data, count, encoded - count,
                    ctx.codec_context->ch_layout.nb_channels, ctx.codec_context->sample_fmt);
            }
            ctx.resampled_frame->nb_samples = encoded;
            ctx.resampled_frame->pts = ctx.last_pts;
            if (!write_frame(ctx, ctx.resampled_frame)) return false;
            ctx.last_pts += encoded;
        }
        return true;
    }

    void FFmpegMuxer::flush_encoders() {
        if (m_audio_stream.codec_context) {
            bool ok = flush_resampler();
            const int64_t end = m_end_timestamp_ns.load();
            if (ok && !m_failed.load() && end >= 0) ok = fill_audio_gap(ns_to_samples(end, m_audio_stream.codec_context->sample_rate));
            if (ok) ok = drain_audio_fifo(true);
            if (ok) ok = write_frame(m_audio_stream, nullptr);
            if (!ok) fail(MEDIACAP_ERROR_ENCODING_FAILED, "Could not flush audio encoder");
        }
        if (m_video_stream.codec_context) {
            int64_t end = m_end_timestamp_ns.load();
            if (end < 0 && m_audio_stream.codec_context)
                end = samples_to_ns(m_audio_stream.audio_tail, m_audio_stream.codec_context->sample_rate);
            if (!m_failed.load() && !advance_video(end)) fail(MEDIACAP_ERROR_ENCODING_FAILED, "Could not flush held video frames");
            if (!write_frame(m_video_stream, nullptr)) fail(MEDIACAP_ERROR_ENCODING_FAILED, "Could not flush video encoder");
        }
    }

    void FFmpegMuxer::finalize() {

        if (m_format_context && !m_header_written) {
            std::lock_guard<std::mutex> lock(m_muxer_mutex);
            if (!m_header_written) {
                int ret = avformat_write_header(m_format_context, nullptr);
                if (ret < 0) {
                    log_ffmpeg_error(ret, "Error writing header for a recording that captured no frames");
                } else {
                    m_header_written = true;
                }
            }
        }

        if (m_format_context && m_header_written) {
            flush_encoders();
            const int result = av_write_trailer(m_format_context);
            if (result < 0) log_ffmpeg_error(result, "Could not write output trailer");
            if (m_format_context->pb) {
                avio_flush(m_format_context->pb);
                if (m_format_context->pb->error < 0)
                    fail(MEDIACAP_ERROR_IO, "Could not flush output file", m_format_context->pb->error);
            }
        }
    }

    void FFmpegMuxer::cleanup() {
        if (m_format_context) {
            if (!(m_format_context->oformat->flags & AVFMT_NOFILE)) {
                const int result = avio_closep(&m_format_context->pb);
                if (result < 0) log_ffmpeg_error(result, "Could not close output file");
            }
            avformat_free_context(m_format_context);
            m_format_context = nullptr;
        }
    }

}

#ifdef _MSC_VER
#pragma warning(pop) 
#endif

