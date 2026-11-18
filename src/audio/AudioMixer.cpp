#include "AudioMixer.h"
#include "utils/RecordingTimeline.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}
namespace MediaCap {
namespace {
constexpr auto jitter_wait = std::chrono::milliseconds(40);
struct MixedOwner {
    std::vector<float> samples;
    uint8_t* plane = nullptr;
    PackedFrame frame;
};
AVSampleFormat sample_format(MediaCap_SampleFormat format) {
    switch (format) {
    case MEDIACAP_SAMPLE_FORMAT_FLT: return AV_SAMPLE_FMT_FLT;
    case MEDIACAP_SAMPLE_FORMAT_S16: return AV_SAMPLE_FMT_S16;
    case MEDIACAP_SAMPLE_FORMAT_S32: return AV_SAMPLE_FMT_S32;
    case MEDIACAP_SAMPLE_FORMAT_U8: return AV_SAMPLE_FMT_U8;
    default: throw std::invalid_argument("Unsupported mixer input sample format");
    }
}
}
struct AudioMixer::Input {
    struct Chunk { int64_t start; std::vector<float> samples; };
    std::deque<Chunk> chunks;
    SwrContext* converter = nullptr;
    MediaCap_AudioFormat format{};
    std::vector<float> scratch;
    int64_t next = 0, coverage_end = 0, anchor_ns = 0, input_frames = 0, correction_at = 0;
    double filtered_error = 0;
    size_t buffered = 0;
    double queued_seconds = 0;
    float gain = 1;
    std::chrono::steady_clock::time_point last_arrival;
    ~Input() { swr_free(&converter); }
};
AudioMixer::AudioMixer(const MediaCap_AudioFormat& format) : m_format(format) {
    if (format.format != MEDIACAP_SAMPLE_FORMAT_FLT || !format.sample_rate || !format.channels)
        throw std::invalid_argument("Mixer output must be packed float with a nonzero rate/channel count");
}
AudioMixer::~AudioMixer() { stop(); }
size_t AudioMixer::add_input(float gain) {
    if (m_thread.joinable() || !std::isfinite(gain) || gain < 0)
        throw std::invalid_argument("Add mixer inputs with finite nonnegative gain before starting");
    auto input = std::make_unique<Input>(); input->gain = gain;
    m_inputs.push_back(std::move(input));
    return m_inputs.size() - 1;
}
void AudioMixer::set_output_callback(OutputCallback callback) { m_callback = std::move(callback); }
void AudioMixer::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_thread.joinable()) return;
    m_stopping = false; m_accepting = true;
    float sum = 0;
    for (auto& input : m_inputs) { sum += input->gain; input->last_arrival = std::chrono::steady_clock::now(); }
    m_gain_divisor = std::max(1.0f, sum);
    m_thread = std::thread(&AudioMixer::run, this);
}
bool AudioMixer::add_frame(size_t id, std::shared_ptr<PackedFrame> frame, int64_t observation) {
    if (!frame || !std::holds_alternative<MediaCap_AudioFrame>(*frame)) return false;
    const auto& audio = std::get<MediaCap_AudioFrame>(*frame);
    if (!audio.format.sample_rate || !audio.format.channels || !audio.data || !audio.data[0]) return false;
    const double seconds = double(audio.num_samples) / audio.format.sample_rate;
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_accepting || id >= m_inputs.size()) return false;
    if (seconds > 2 || m_inputs[id]->queued_seconds + seconds > 2.001) {
        const auto previous = m_dropped.fetch_add(audio.num_samples);
        lock.unlock();
        if (!previous) errors.report(MEDIACAP_ERROR_BUFFER_OVERFLOW, "Audio mixer input buffer overflow; samples were lost");
        return false;
    }
    m_inputs[id]->queued_seconds += seconds;
    m_events.push_back({id, std::move(frame), observation, std::chrono::steady_clock::now()});
    m_ready.notify_one();
    return true;
}
void AudioMixer::stop(int64_t end_ns) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false; m_stopping = true;
        if (end_ns >= 0) m_end = ns_to_samples(end_ns, m_format.sample_rate);
    }
    m_ready.notify_one();
    if (m_thread.joinable()) m_thread.join();
}
void AudioMixer::append(Input& input, int frames) {
    if (frames <= 0) return;
    const int skip = int(std::min<int64_t>(frames,
        std::max<int64_t>(0, std::max(input.coverage_end, m_cursor) - input.next)));
    const int kept = frames - skip;
    if (input.buffered + kept > size_t(m_format.sample_rate) * 2) {
        m_dropped.fetch_add(kept);
    } else if (kept) {
        input.chunks.push_back({input.next + skip, std::vector<float>(
            input.scratch.begin() + size_t(skip) * m_format.channels,
            input.scratch.begin() + size_t(frames) * m_format.channels)});
        input.buffered += kept;
    }
    input.next += frames;
    input.coverage_end = std::max(input.coverage_end, input.next);
}
void AudioMixer::flush(Input& input) {
    if (!input.converter) return;
    for (;;) {
        const int capacity = std::max(256, swr_get_out_samples(input.converter, 0));
        input.scratch.resize(size_t(capacity) * m_format.channels);
        uint8_t* output = reinterpret_cast<uint8_t*>(input.scratch.data());
        const int frames = swr_convert(input.converter, &output, capacity, nullptr, 0);
        if (frames < 0) throw std::runtime_error("Mixer resampler flush failed");
        if (!frames) break;
        append(input, frames);
    }
}
void AudioMixer::convert(Input& input, const Event& event) {
    const auto& frame = std::get<MediaCap_AudioFrame>(*event.frame);
    input.last_arrival = event.arrived;
    const int64_t expected_ns = input.anchor_ns + (input.format.sample_rate ?
        samples_to_ns(input.input_frames, input.format.sample_rate) : 0);
    const bool discontinuity = input.converter && std::abs(frame.timestamp_ns - expected_ns) >
        samples_to_ns(2, frame.format.sample_rate);
    if (!input.converter || input.format.sample_rate != frame.format.sample_rate ||
        input.format.channels != frame.format.channels || input.format.format != frame.format.format || discontinuity) {
        flush(input); swr_free(&input.converter);
        input.format = frame.format;
        AVChannelLayout in{}, out{};
        av_channel_layout_default(&in, frame.format.channels);
        av_channel_layout_default(&out, m_format.channels);
        const int result = swr_alloc_set_opts2(&input.converter, &out, AV_SAMPLE_FMT_FLT, m_format.sample_rate,
            &in, sample_format(frame.format.format), frame.format.sample_rate, 0, nullptr);
        av_channel_layout_uninit(&in); av_channel_layout_uninit(&out);
        if (result < 0 || !input.converter || swr_init(input.converter) < 0)
            throw std::runtime_error("Mixer resampler initialization failed");
        input.next = std::max<int64_t>(0, av_rescale_q(frame.timestamp_ns, AVRational{1, 1000000000},
            AVRational{1, int(m_format.sample_rate)}));
        input.anchor_ns = frame.timestamp_ns; input.input_frames = 0;
        input.filtered_error = 0; input.correction_at = input.next + m_format.sample_rate;
    }
    if (event.observation >= 0) {
        const int64_t pending = swr_get_delay(input.converter, m_format.sample_rate);
        const double error = double(ns_to_samples(event.observation, m_format.sample_rate) - input.next - pending);
        input.filtered_error += 0.01 * (error - input.filtered_error);
        if (input.next >= input.correction_at) {
            const int distance = int(m_format.sample_rate) * 2;
            const int limit = std::max(1, distance / 2000);
            const int correction = std::abs(input.filtered_error) > m_format.sample_rate * 0.002 ?
                int(std::clamp(input.filtered_error, -double(limit), double(limit))) : 0;
            if (swr_set_compensation(input.converter, correction, distance) < 0)
                throw std::runtime_error("Mixer drift compensation failed");
            input.correction_at = input.next + m_format.sample_rate;
        }
    }
    const int capacity = swr_get_out_samples(input.converter, int(frame.num_samples));
    if (capacity < 0) throw std::runtime_error("Invalid resampler output size");
    input.scratch.resize(size_t(capacity) * m_format.channels);
    uint8_t* output = reinterpret_cast<uint8_t*>(input.scratch.data());
    const uint8_t* data = frame.data[0];
    const int count = swr_convert(input.converter, &output, capacity, &data, int(frame.num_samples));
    if (count < 0) throw std::runtime_error("Mixer resampling failed");
    append(input, count);
    input.input_frames += frame.num_samples;
}
bool AudioMixer::emit(bool finishing) {
    int64_t end = 0;
    for (auto& input : m_inputs) end = std::max(end, input->coverage_end);
    const int64_t cutoff = m_end.load();
    if (cutoff >= 0) end = finishing ? cutoff : std::min(end, cutoff);
    if (end <= m_cursor) return false;
    const int64_t count = std::min<int64_t>(std::max<uint32_t>(1, m_format.sample_rate / 100), end - m_cursor);
    const auto now = std::chrono::steady_clock::now();
    if (!finishing) {
        if (count < int64_t(std::max<uint32_t>(1, m_format.sample_rate / 100))) return false;
        for (auto& input : m_inputs)
            if (input->coverage_end < m_cursor + count && now - input->last_arrival < jitter_wait) return false;
    }
    auto owner = std::make_shared<MixedOwner>();
    owner->samples.assign(size_t(count) * m_format.channels, 0);
    for (auto& input : m_inputs) {
        for (const auto& chunk : input->chunks) {
            const int64_t chunk_end = chunk.start + chunk.samples.size() / m_format.channels;
            const int64_t begin = std::max(m_cursor, chunk.start), last = std::min(m_cursor + count, chunk_end);
            if (last <= begin) continue;
            const size_t destination = size_t(begin - m_cursor) * m_format.channels;
            const size_t source = size_t(begin - chunk.start) * m_format.channels;
            for (size_t i = 0; i < size_t(last - begin) * m_format.channels; ++i)
                owner->samples[destination + i] += chunk.samples[source + i] * input->gain / m_gain_divisor;
        }
        while (!input->chunks.empty() && input->chunks.front().start +
            int64_t(input->chunks.front().samples.size() / m_format.channels) <= m_cursor + count) {
            input->buffered -= input->chunks.front().samples.size() / m_format.channels;
            input->chunks.pop_front();
        }
    }
    for (auto& sample : owner->samples) sample = std::clamp(sample, -1.0f, 1.0f);
    owner->plane = reinterpret_cast<uint8_t*>(owner->samples.data());
    owner->frame.emplace<MediaCap_AudioFrame>(MediaCap_AudioFrame{&owner->plane,
        uint32_t(count), m_format, samples_to_ns(m_cursor, m_format.sample_rate)});
    m_cursor += count;
    if (m_callback) m_callback(std::shared_ptr<PackedFrame>(owner, &owner->frame));
    return true;
}
void AudioMixer::run() {
    try {
        for (;;) {
            Event event{};
            bool have_event = false, finishing = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_events.empty() && !m_stopping)
                    m_ready.wait_for(lock, jitter_wait, [&] { return !m_events.empty() || m_stopping; });
                if (!m_events.empty()) {
                    event = std::move(m_events.front()); m_events.pop_front(); have_event = true;
                    const auto& frame = std::get<MediaCap_AudioFrame>(*event.frame);
                    m_inputs[event.input]->queued_seconds -= double(frame.num_samples) / frame.format.sample_rate;
                }
                finishing = m_stopping && !have_event;
            }
            if (have_event) convert(*m_inputs[event.input], event);
            if (finishing) {
                for (auto& input : m_inputs) flush(*input);
                while (emit(true)) {}
                break;
            }
            while (emit(false)) {}
        }
    } catch (const std::exception& error) {
        m_failed.store(true);
        errors.report(MEDIACAP_ERROR_ENCODING_FAILED, error.what());
    } catch (...) {
        m_failed.store(true); errors.exception("Unexpected audio mixer exception");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_accepting = false;
    if (m_dropped.load()) std::cerr << "MediaCap audio mixer: dropped " << m_dropped.load()
        << " frames because the two-second input buffer was full." << std::endl;
}
}
