#include "frame_sinks.hpp"
#include "output_path.hpp"
#include "../cli.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <limits>
#include <mutex>
#include <set>
#include <thread>

namespace mccli {
namespace {
constexpr size_t buffer_limit = 128 * 1024 * 1024;

struct Packet {
    bool video;
    std::string metadata;
    std::vector<uint8_t> bytes;
};

std::string packet_header(const char* kind, size_t index, MediaCap_SourceType source, int64_t timestamp) {
    return "{\"type\":" + quote(kind) + ",\"source_index\":" + std::to_string(index) +
        ",\"source_type\":" + std::to_string(source) + ",\"timestamp_ns\":" + std::to_string(timestamp);
}

struct Plane {
    uint64_t row_bytes = 0;
    uint32_t rows = 0;
};

std::array<Plane, 3> video_planes(const MediaCap_VideoFrameView& frame) {
    const uint64_t width = frame.width;
    const uint32_t height = frame.height;
    const auto chroma_rows = height / 2 + height % 2;
    switch (frame.pixel_format) {
    case MEDIACAP_PIXEL_FORMAT_BGRA:
    case MEDIACAP_PIXEL_FORMAT_RGBA: return {{{width * 4, height}}};
    case MEDIACAP_PIXEL_FORMAT_BGR24:
    case MEDIACAP_PIXEL_FORMAT_RGB24: return {{{width * 3, height}}};
    case MEDIACAP_PIXEL_FORMAT_NV12: return {{{width, height}, {((width + 1) / 2) * 2, chroma_rows}}};
    case MEDIACAP_PIXEL_FORMAT_I420:
        return {{{width, height}, {(width + 1) / 2, chroma_rows}, {(width + 1) / 2, chroma_rows}}};
    default: throw std::runtime_error("Cannot export an unknown video pixel format.");
    }
}

Packet pack_video(size_t index, MediaCap_SourceType source, const MediaCap_VideoFrameView& frame) {
    Packet packet{true, packet_header("video", index, source, frame.timestamp_ns), {}};
    packet.metadata += ",\"frame_index\":" + std::to_string(frame.frame_index) + ",\"width\":" + std::to_string(frame.width) +
        ",\"height\":" + std::to_string(frame.height) + ",\"pixel_format\":" + quote(pixel_name(frame.pixel_format)) +
        ",\"orientation\":" + quote(frame.orientation == MEDIACAP_ORIENTATION_BOTTOM_UP ? "bottom-up" : "top-down") + ",\"planes\":[";
    const auto planes = video_planes(frame);
    size_t total = 0;
    for (size_t i = 0; i < planes.size() && planes[i].rows; ++i) {
        const auto plane = planes[i];
        const int64_t stride = frame.linesize[i];
        const uint64_t pitch = stride < 0 ? -stride : stride;
        if (!frame.data[i] || !plane.row_bytes || plane.row_bytes > pitch ||
            pitch * (plane.rows - 1) > static_cast<uint64_t>(std::numeric_limits<std::ptrdiff_t>::max()))
            throw std::runtime_error("Cannot export a video frame with an invalid plane layout.");
        if (plane.row_bytes > (buffer_limit - total) / plane.rows)
            throw std::runtime_error("Video frame exceeds the raw output buffer limit.");
        total += static_cast<size_t>(plane.row_bytes * plane.rows);
        if (i) packet.metadata += ',';
        packet.metadata += "{\"row_bytes\":" + std::to_string(plane.row_bytes) + ",\"rows\":" + std::to_string(plane.rows) + '}';
    }
    packet.metadata += "],\"payload_bytes\":" + std::to_string(total) + "}\n";
    packet.bytes.reserve(total);
    for (size_t i = 0; i < planes.size() && planes[i].rows; ++i) {
        const auto plane = planes[i];
        for (uint32_t y = 0; y < plane.rows; ++y) {
            const auto* row = frame.data[i] + static_cast<std::ptrdiff_t>(y) * frame.linesize[i];
            packet.bytes.insert(packet.bytes.end(), row, row + static_cast<size_t>(plane.row_bytes));
        }
    }
    return packet;
}

struct SampleLayout {
    const char* name;
    size_t bytes;
};

SampleLayout sample_layout(MediaCap_SampleFormat format) {
    switch (format) {
    case MEDIACAP_SAMPLE_FORMAT_U8: return {"u8", 1};
    case MEDIACAP_SAMPLE_FORMAT_S16: return {"s16le", 2};
    case MEDIACAP_SAMPLE_FORMAT_S32: return {"s32le", 4};
    case MEDIACAP_SAMPLE_FORMAT_FLT: return {"f32le", 4};
    case MEDIACAP_SAMPLE_FORMAT_DBL: return {"f64le", 8};
    default: throw std::runtime_error("Cannot export a non-interleaved audio sample format.");
    }
}

Packet pack_audio(size_t index, MediaCap_SourceType source, const MediaCap_AudioFrameView& frame) {
    const auto layout = sample_layout(frame.format.format);
    const uint64_t bytes = uint64_t(frame.num_samples) * frame.format.channels * layout.bytes;
    if (!frame.data || !bytes) throw std::runtime_error("Cannot export an empty audio block.");
    if (bytes > buffer_limit) throw std::runtime_error("Audio block exceeds the raw output buffer limit.");
    Packet packet{false, packet_header("audio", index, source, frame.timestamp_ns), {frame.data, frame.data + bytes}};
    packet.metadata += ",\"sample_frames\":" + std::to_string(frame.num_samples) + ",\"channels\":" +
        std::to_string(frame.format.channels) + ",\"sample_rate\":" + std::to_string(frame.format.sample_rate) +
        ",\"sample_format\":" + quote(layout.name) + ",\"payload_bytes\":" + std::to_string(bytes) + "}\n";
    const uint16_t native_one = 1;
    if (layout.bytes > 1 && *reinterpret_cast<const unsigned char*>(&native_one) != 1)
        for (size_t i = 0; i < packet.bytes.size(); i += layout.bytes)
            std::reverse(packet.bytes.data() + i, packet.bytes.data() + i + layout.bytes);
    return packet;
}

void write_packet(const OutputPath& path, std::ofstream& file, const Packet& packet) {
    if (!file.is_open()) {
        file.open(path.path(), std::ios::binary | std::ios::trunc);
        file << "MediaCap frames 1\n";
    }
    file.write(packet.metadata.data(), static_cast<std::streamsize>(packet.metadata.size()));
    file.write(reinterpret_cast<const char*>(packet.bytes.data()), static_cast<std::streamsize>(packet.bytes.size()));
    if (!file) throw std::runtime_error("Cannot write raw output file: " + path.path().u8string());
}
}

struct FrameSinks::Impl {
    const OutputPath* video_path;
    const OutputPath* audio_path;
    std::atomic<uint64_t> video{0}, audio{0}, samples{0};
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::deque<Packet> pending;
    std::set<size_t> startup_sources;
    size_t buffered = 0;
    bool writing = false;
    bool stopping = false;
    std::exception_ptr failure;
    std::thread writer;

    Impl(const OutputPath* video_output, const OutputPath* audio_output) : video_path(video_output), audio_path(audio_output) {
        if (video_path || audio_path) writer = std::thread([this] { write(); });
    }

    ~Impl() { close(); }

    void close() {
        if (!writer.joinable()) return;
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        ready.notify_one();
        writer.join();
    }

    void fail(std::exception_ptr error) noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        if (!failure) failure = std::move(error);
        pending.clear();
        buffered = 0;
        ready.notify_one();
    }

    bool accepts(size_t source) {
        std::lock_guard<std::mutex> lock(mutex);
        return !failure && (writing || startup_sources.insert(source).second);
    }

    void push(Packet packet) {
        const auto size = packet.bytes.size() + packet.metadata.size();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (failure) return;
            if (size > buffer_limit - buffered)
                throw std::runtime_error("Raw output could not keep up. Use a faster disk or lower the capture rate or size.");
            pending.push_back(std::move(packet));
            buffered += size;
        }
        ready.notify_one();
    }

    void write() noexcept {
        try {
            std::ofstream video_file, audio_file;
            for (;;) {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [&] { return stopping || failure || (writing && !pending.empty()); });
                if (failure || !writing || pending.empty()) break;
                auto packet = std::move(pending.front());
                pending.pop_front();
                buffered -= packet.bytes.size() + packet.metadata.size();
                lock.unlock();
                if (packet.video) write_packet(*video_path, video_file, packet);
                else write_packet(*audio_path, audio_file, packet);
            }
            if (video_file.is_open()) {
                video_file.close();
                if (!video_file) throw std::runtime_error("Cannot finish the video frame output file.");
            }
            if (audio_file.is_open()) {
                audio_file.close();
                if (!audio_file) throw std::runtime_error("Cannot finish the audio sample output file.");
            }
        } catch (...) { fail(std::current_exception()); }
    }
};

FrameSinks::FrameSinks(const OutputPath* video, const OutputPath* audio) : impl_(std::make_unique<Impl>(video, audio)) {}
FrameSinks::~FrameSinks() = default;

void FrameSinks::video_callback(size_t index, MediaCap_SourceType source, const MediaCap_VideoFrameView* view, void* data) noexcept {
    auto& state = *static_cast<FrameSinks*>(data)->impl_;
    ++state.video;
    if (!state.video_path) return;
    try { if (state.accepts(index)) state.push(pack_video(index, source, *view)); }
    catch (...) { state.fail(std::current_exception()); }
}

void FrameSinks::audio_callback(size_t index, MediaCap_SourceType source, const MediaCap_AudioFrameView* view, void* data) noexcept {
    auto& state = *static_cast<FrameSinks*>(data)->impl_;
    ++state.audio;
    state.samples += view->num_samples;
    if (!state.audio_path) return;
    try { if (state.accepts(index)) state.push(pack_audio(index, source, *view)); }
    catch (...) { state.fail(std::current_exception()); }
}

FrameCounts FrameSinks::counts() const { return {impl_->video.load(), impl_->audio.load(), impl_->samples.load()}; }

bool FrameSinks::failed() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return static_cast<bool>(impl_->failure);
}

void FrameSinks::begin_writing() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->writing = true;
    }
    impl_->ready.notify_one();
}

void FrameSinks::finish() {
    impl_->close();
    if (impl_->failure) std::rethrow_exception(impl_->failure);
}
}
