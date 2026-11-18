#ifndef MEDIACAP_FFMPEGMUXER_H
#define MEDIACAP_FFMPEGMUXER_H

#include "capture/Capturer.h"
#include "utils/ThreadSafeQueue.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;
struct AVAudioFifo;

namespace MediaCap {

class FFmpegMuxer {
private:
    struct StreamContext {
        AVCodecContext* codec_context = nullptr;
        AVStream* stream = nullptr;
        AVFrame* frame = nullptr;
        AVPacket* packet = nullptr;
        SwsContext* sws_context = nullptr;
        SwrContext* swr_context = nullptr;
        AVFrame* resampled_frame = nullptr;
        AVAudioFifo* audio_fifo = nullptr;
        int resampled_capacity = 0;
        int64_t last_pts = -1;
        AVFrame* scratch = nullptr;
        MediaCap_AudioFormat input_format{};
        int64_t input_anchor_ns = 0, input_samples = 0, converted_next = 0, audio_tail = 0;
        std::vector<uint8_t*> shifted_planes;
        ~StreamContext();
    };

public:
    Errors::Reporter errors{MEDIACAP_ERROR_COMPONENT_ENCODER};
    explicit FFmpegMuxer(const MediaCap_RecordingConfig& config);
    static bool validate_format(const MediaCap_RecordingConfig&, const Errors::Reporter&);
    ~FFmpegMuxer();

    FFmpegMuxer(const FFmpegMuxer&) = delete;
    FFmpegMuxer& operator=(const FFmpegMuxer&) = delete;

    bool start();
    void stop(int64_t end_timestamp_ns = -1);
    bool add_frame(std::shared_ptr<PackedFrame> frame);
    bool is_running() const { return m_is_running.load(); }
    bool healthy() const { return !m_failed.load(); }
    // Configure before start. Live sessions supply paused-adjusted media time;
    // offline callers may omit it and let incoming timestamps drive pacing.
    void set_media_clock(std::function<int64_t()> clock) { m_media_clock = std::move(clock); }
    int64_t video_ticks_written() const { return m_video_next_pts.load(); }

private:
    void fail(MediaCap_ErrorCode code, const char* message, int64_t native = 0);
    void log_ffmpeg_error(int err, const std::string& message);
    bool initialize();
    bool initialize_video_stream();
    bool initialize_audio_stream();
    void set_default_codecs();
    void muxer_thread_func();
    bool process_frame(const std::shared_ptr<PackedFrame>& frame);
    bool encode_video_frame(const MediaCap_VideoFrame& video_frame);
    bool encode_audio_frame(const MediaCap_AudioFrame& audio_frame);
    bool write_frame(StreamContext& context, AVFrame* frame_to_encode);
    bool drain_audio_fifo(bool final = false);
    bool append_audio(uint8_t** data, int frames, int64_t start);
    bool push_audio(uint8_t** data, int frames);
    bool fill_audio_gap(int64_t target);
    bool flush_resampler();
    bool prepare_scratch(int capacity);
    bool advance_video(int64_t end_ns);
    void flush_encoders();
    void finalize();
    void cleanup();

    MediaCap_RecordingConfig m_config_storage;
    MediaCap_VideoEncoderConfig m_video_config_storage;
    MediaCap_AudioEncoderConfig m_audio_config_storage;
    
    std::atomic<bool> m_is_running{false};
    std::atomic<bool> m_failed{false};
    std::atomic<int64_t> m_end_timestamp_ns{-1};
    std::atomic<int64_t> m_video_next_pts{0};
    std::function<int64_t()> m_media_clock;
    bool m_video_has_frame = false;
    std::mutex m_admission_mutex;
    std::string m_output_filename;
    size_t m_queued_bytes = 0;
    bool m_header_written = false;
    
    AVFormatContext* m_format_context = nullptr;
    StreamContext m_video_stream;
    StreamContext m_audio_stream;

    std::thread m_muxer_thread;
    Utils::ThreadSafeQueue<std::shared_ptr<PackedFrame>> m_frame_queue;
    
    std::mutex m_muxer_mutex;
};

}

#endif
