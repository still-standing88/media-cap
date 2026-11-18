#include "capture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <stdexcept>
#include <utility>

namespace audiopix {
namespace fs = std::filesystem;
namespace {

std::string message(const MediaCap_Error& error, std::string fallback) {
    return error.message[0] ? error.message : std::move(fallback);
}

std::string legacy_id(const MediaCap_AudioDevice& device) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "legacy:";
    for (uint8_t byte : device.id) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}

void add_source(MediaCap::Session& session, const Source& source, int rate, int channels) {
    const auto options = discover(source.kind);
    const auto selected = std::ranges::find(options, source.id, &Candidate::id);
    if (selected == options.end() || selected->name != source.name)
        throw std::runtime_error("Source is unavailable. Edit it to select a current device: " + source.name);

    MediaCap::clear_last_error();
    const auto interval = std::max(1, 1000 / source.fps);
    const auto audio = MediaCap_AudioFormat{MEDIACAP_SAMPLE_FORMAT_FLT, uint32_t(rate), uint8_t(channels)};
    const auto routed = [&](MediaCap_AudioCaptureKind kind) {
        auto config = MediaCap::default_audio_source_config();
        config.kind = kind;
        config.sample_rate = audio.sample_rate;
        config.channels = audio.channels;
        config.gain = source.gain / 100.f;
        return config;
    };
    std::visit([&](const auto& device) {
        using Type = std::decay_t<decltype(device)>;
        if constexpr (std::is_same_v<Type, MediaCap_Camera>) {
            session.add_source(device, {uint32_t(source.width), uint32_t(source.height), source.fps,
                                        MEDIACAP_PIXEL_FORMAT_BGRA});
        } else if constexpr (std::is_same_v<Type, MediaCap_Monitor>) {
            session.add_source(device, MediaCap::ScreenCaptureConfig{interval});
        } else if constexpr (std::is_same_v<Type, MediaCap_Window>) {
            session.add_source(device, MediaCap::WindowCaptureConfig{interval});
        } else if constexpr (std::is_same_v<Type, MediaCap_AudioDevice>) {
            if (source.gain != 100)
                throw std::runtime_error("This audio backend requires a source gain of 100%.");
            session.add_source(device, audio);
        } else if constexpr (std::is_same_v<Type, MediaCap::AudioCaptureDevice>) {
            auto config = routed(device.kind);
            config.device_id = device.id.c_str();
            if (!session.add_audio_source(config))
                throw std::runtime_error(message(MediaCap::get_last_error(), "Cannot add the selected audio device."));
        } else {
            auto config = routed(MEDIACAP_AUDIO_CAPTURE_PROCESS);
            config.process_id = device.pid;
            if (!session.add_audio_source(config))
                throw std::runtime_error(message(MediaCap::get_last_error(), "Cannot add the selected process."));
        }
    }, selected->device);
    const auto error = MediaCap::get_last_error();
    if (error.code != MEDIACAP_ERROR_NONE)
        throw std::runtime_error(message(error, "Cannot add the selected source."));
}

fs::path next_recording(const std::string& output) {
    if (output.empty()) throw std::runtime_error("Set an output directory in Preferences.");
    const fs::path directory(std::u8string(output.begin(), output.end()));
    fs::create_directories(directory);
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::random_device random;
    for (int attempt = 0; attempt < 10; ++attempt) {
        auto file = directory / ("AudioPix-" + std::to_string(stamp) + "-" + std::to_string(random()) + ".mkv");
        if (!fs::exists(file)) return file;
    }
    throw std::runtime_error("Could not select an available recording filename.");
}

} // namespace

std::vector<Candidate> discover(Kind kind) {
    MediaCap::clear_last_error();
    std::vector<Candidate> result;
    switch (kind) {
    case Kind::Camera:
        for (const auto& camera : MediaCap::get_camera_devices()) {
            const auto id = camera.unique_id[0] ? "device:" + std::string(camera.unique_id)
                                                : "index:" + std::to_string(camera.index);
            result.push_back({id, camera.name, camera});
        }
        break;
    case Kind::Monitor:
        for (const auto& monitor : MediaCap::get_monitors())
            result.push_back({std::to_string(monitor.id), monitor.name, monitor});
        break;
    case Kind::Window:
        for (const auto& window : MediaCap::get_windows())
            result.push_back({std::to_string(window.handle), window.name, window});
        break;
    case Kind::Process: {
        const auto capabilities = MediaCap::get_audio_capabilities();
        if (!capabilities.backend_enabled || capabilities.process == MEDIACAP_AUDIO_UNAVAILABLE)
            throw std::runtime_error(capabilities.process_detail[0] ? capabilities.process_detail :
                                     "Process audio is unavailable on this build or platform.");
        for (const auto& process : MediaCap::get_audio_processes())
            result.push_back({std::to_string(process.pid), process.name.empty() ? process.executable : process.name, process});
        break;
    }
    case Kind::Input:
    case Kind::Loopback: {
        const bool mic = kind == Kind::Input;
        const auto caps = MediaCap::get_audio_capabilities();
        if (caps.backend_enabled && (mic ? caps.microphone : caps.system) != MEDIACAP_AUDIO_UNAVAILABLE) {
            const auto type = mic ? MEDIACAP_AUDIO_CAPTURE_MIC : MEDIACAP_AUDIO_CAPTURE_SYSTEM;
            for (const auto& device : MediaCap::get_audio_capture_devices())
                if (device.kind == type)
                    result.push_back({"flex:" + device.id, device.name, device});
        }
        const auto legacy_type = mic ? MEDIACAP_AUDIO_DEVICE_TYPE_INPUT : MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK;
        if (result.empty() && MediaCap::get_last_error().code == MEDIACAP_ERROR_NONE) {
            for (const auto& device : MediaCap::get_audio_devices())
                if (device.type == legacy_type)
                    result.push_back({legacy_id(device), device.name, device});
        }
        break;
    }
    }
    const auto error = MediaCap::get_last_error();
    if (result.empty() && error.code != MEDIACAP_ERROR_NONE)
        throw std::runtime_error(message(error, "MediaCap could not discover sources."));
    return result;
}

bool supports_gain(const Candidate& candidate) {
    return std::holds_alternative<MediaCap::AudioCaptureDevice>(candidate.device)
        || std::holds_alternative<MediaCap::AudioProcess>(candidate.device);
}

void Capture::start_recording(const std::vector<Source>& sources, const Settings& settings) {
    if (sources.empty()) throw std::runtime_error("Add a capture source before recording.");
    auto session = std::make_unique<MediaCap::Session>();
    bool with_audio = false;
    bool with_video = false;
    for (const auto& source : sources) {
        if (video_source(source.kind)) {
            if (with_video) throw std::runtime_error("A MediaCap recording supports one video source.");
            with_video = true;
        } else {
            with_audio = true;
        }
        add_source(*session, source, settings.rate, settings.channels);
    }
    auto file = next_recording(settings.output);
    const auto filename = utf8_path(file);
    MediaCap::VideoEncoderConfig video{uint32_t(settings.width), uint32_t(settings.height), settings.fps,
                                      int64_t(settings.video_kbps) * 1000, MediaCap_VideoCodec(settings.video_codec)};
    MediaCap::AudioEncoderConfig audio{uint32_t(settings.rate), uint8_t(settings.channels),
                                      int64_t(settings.audio_kbps) * 1000, MediaCap_AudioCodec(settings.audio_codec)};
    const MediaCap::RecordingConfig config{filename.c_str(), MEDIACAP_CONTAINER_MKV,
        with_video ? &video : nullptr, with_audio ? &audio : nullptr};
    MediaCap::clear_last_error();
    if (!MediaCap_ValidateRecordingFormat(&config))
        throw std::runtime_error(message(MediaCap::get_last_error(), "The selected recording format is unavailable."));
    if (!session->start_recording(config)) {
        auto reason = message(session->get_error(), message(MediaCap::get_last_error(), "Could not start recording."));
        session.reset();
        std::error_code ignored;
        fs::remove(file, ignored);
        throw std::runtime_error(std::move(reason));
    }
    recording_path = std::move(file);
    active = std::move(session);
}

void Capture::start_preview(const Source& source) {
    auto session = std::make_unique<MediaCap::Session>();
    add_source(*session, source, 48000, 2);
    const bool video = video_source(source.kind);
    if (!session->set_frame_callbacks(video ? nullptr : preview_audio, video ? preview_video : nullptr, this) ||
        !session->start_capture())
        throw std::runtime_error(message(session->get_error(), "Could not start the preview."));
    active = std::move(session);
}

std::string Capture::finish_recording(bool cancel) {
    active->stop_recording();
    const auto error = failure();
    active.reset();
    const auto file = std::exchange(recording_path, {});
    if (cancel) {
        fs::remove(file);
        return "Recording canceled";
    }
    const auto output = utf8_path(file);
    return error.empty() ? "Saved " + output : error + " · Partial recording: " + output;
}

void Capture::stop_preview() {
    active->stop_capture();
    active.reset();
    take_image();
    take_level();
}

bool Capture::running() const { return active && active->is_capturing(); }
bool Capture::paused() const { return active && active->is_paused(); }

bool Capture::toggle_pause() {
    active->set_paused(!active->is_paused());
    return active->is_paused();
}

std::string Capture::failure() const {
    return active ? message(active->get_error(), {}) : std::string{};
}

PreviewImage Capture::take_image() {
    std::lock_guard lock(frame_mutex);
    return std::exchange(frame, {});
}

float Capture::take_level() { return level.exchange(0); }

void Capture::preview_video(size_t, MediaCap_SourceType, const MediaCap_VideoFrameView* view, void* context) {
    auto& self = *static_cast<Capture*>(context);
    if (!view->data[0] || !view->width || !view->height) return;
    const bool bgra = view->pixel_format == MEDIACAP_PIXEL_FORMAT_BGRA;
    if (!bgra && view->pixel_format != MEDIACAP_PIXEL_FORMAT_RGBA) return;
    const auto stride = std::abs(int64_t(view->linesize[0]));
    if (stride < int64_t(view->width) * 4) return;
    std::lock_guard lock(self.frame_mutex);
    if (!self.frame.rgba.empty()) return;
    const unsigned step = std::max(1u, (view->width - 1) / 640 + 1);
    const unsigned width = (view->width - 1) / step + 1;
    const unsigned height = (view->height - 1) / step + 1;
    PreviewImage image{width, height, std::vector<uint8_t>(size_t(width) * height * 4)};
    for (unsigned y = 0; y < height; ++y) {
        const auto row = view->orientation == MEDIACAP_ORIENTATION_BOTTOM_UP ? view->height - 1 - y * step : y * step;
        const auto* input = view->data[0] + ptrdiff_t(row) * view->linesize[0];
        auto* output = image.rgba.data() + size_t(y) * width * 4;
        for (unsigned x = 0; x < width; ++x) {
            const auto* pixel = input + size_t(x) * step * 4;
            *output++ = pixel[bgra ? 2 : 0];
            *output++ = pixel[1];
            *output++ = pixel[bgra ? 0 : 2];
            *output++ = 255;
        }
    }
    self.frame = std::move(image);
}

void Capture::preview_audio(size_t, MediaCap_SourceType, const MediaCap_AudioFrameView* view, void* context) {
    auto& self = *static_cast<Capture*>(context);
    if (!view->data || view->format.format != MEDIACAP_SAMPLE_FORMAT_FLT) return;
    float peak = 0;
    for (size_t i = 0, samples = size_t(view->num_samples) * view->format.channels; i < samples; ++i) {
        float sample;
        std::memcpy(&sample, view->data + i * sizeof(sample), sizeof(sample));
        peak = std::max(peak, std::abs(sample));
    }
    peak = std::min(peak, 1.f);
    auto current = self.level.load();
    while (peak > current && !self.level.compare_exchange_weak(current, peak)) {}
}

} // namespace audiopix
