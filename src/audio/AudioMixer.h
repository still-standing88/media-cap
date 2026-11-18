#pragma once
#include "capture/Capturer.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace MediaCap {
class AudioMixer {
public:
    Errors::Reporter errors{MEDIACAP_ERROR_COMPONENT_MIXER};
    using OutputCallback = std::function<void(std::shared_ptr<PackedFrame>)>;
    explicit AudioMixer(const MediaCap_AudioFormat& output_format);
    ~AudioMixer();
    size_t add_input(float gain = 1.0f);
    bool add_frame(size_t input, std::shared_ptr<PackedFrame> frame, int64_t observed_start_ns = -1);
    void set_output_callback(OutputCallback callback);
    void start();
    void stop(int64_t end_timestamp_ns = -1);
    uint64_t dropped_frames() const { return m_dropped.load(); }
    bool healthy() const { return !m_failed.load(); }
private:
    struct Input;
    struct Event {
        size_t input;
        std::shared_ptr<PackedFrame> frame;
        int64_t observation;
        std::chrono::steady_clock::time_point arrived;
    };
    void run();
    void convert(Input& input, const Event& event);
    void flush(Input& input);
    void append(Input& input, int frames);
    bool emit(bool finishing);
    MediaCap_AudioFormat m_format;
    std::vector<std::unique_ptr<Input>> m_inputs;
    OutputCallback m_callback;
    std::deque<Event> m_events;
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::thread m_thread;
    bool m_accepting = false, m_stopping = false;
    int64_t m_cursor = 0;
    std::atomic<int64_t> m_end{-1};
    float m_gain_divisor = 1.0f;
    std::atomic<uint64_t> m_dropped{0};
    std::atomic<bool> m_failed{false};
};
}
