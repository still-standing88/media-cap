#include "../cli.hpp"
#include "../platform/control.hpp"
#include "frame_sinks.hpp"
#include "output_path.hpp"
#include "sources.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace mccli {
namespace {
class PreparationDeadline {
    std::mutex mutex_;
    std::condition_variable changed_;
    bool finished_ = false;
    std::thread worker_;
public:
    PreparationDeadline(const std::array<OutputPath*, 3>& paths, Output& out) : worker_([&, this] {
        std::unique_lock<std::mutex> lock(mutex_);
        if (changed_.wait_for(lock, std::chrono::seconds(20), [&] { return finished_; })) return;
        // A driver can block both startup and destruction. At this deadline no
        // recording has been acknowledged; release only our empty reservation
        // and exit without unwinding through that same blocked driver again.
        for (auto* path : paths) if (path) path->remove_empty_reservation();
        try { out.error("Capture preparation timed out. Check the selected devices and retry.", "\"exit_code\":1,\"code\":" + std::to_string(MEDIACAP_ERROR_TIMEOUT)); }
        catch (...) {}
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(), 1);
#endif
        std::_Exit(1);
    }) {}
    void finish() {
        { std::lock_guard<std::mutex> lock(mutex_); finished_ = true; }
        changed_.notify_one();
        if (worker_.joinable()) worker_.join();
    }
    ~PreparationDeadline() { finish(); }
};
std::string counts_json(const FrameCounts& counts) {
    return "\"video_frames\":" + std::to_string(counts.video) + ",\"audio_blocks\":" + std::to_string(counts.audio) +
        ",\"sample_frames\":" + std::to_string(counts.samples);
}

std::string counts_text(const FrameCounts& counts) {
    return std::to_string(counts.video) + " video frames, " + std::to_string(counts.audio) + " audio blocks, " +
        std::to_string(counts.samples) + " sample frames.";
}

MediaCap::Resolution add_video(MediaCap::Session& session, const RecordingOptions& o) {
    if (o.kind == "screen") {
        const auto monitor = select_monitor(o.monitor);
        session.add_source(monitor, MediaCap::ScreenCaptureConfig{o.capture_interval});
        check_error();
        return {monitor.width, monitor.height};
    }
    if (o.kind == "window") {
        const auto window = select_window(o.window, o.window_title);
        session.add_source(window, MediaCap::WindowCaptureConfig{o.capture_interval});
        check_error();
        return {window.width, window.height};
    }
    if (o.kind == "camera") {
        session.add_source(select_camera(o.camera),
            MediaCap::CameraCaptureConfig{o.camera_size.width, o.camera_size.height, o.capture_fps, o.pixel_value});
        check_error();
        return o.camera_size;
    }
    return {};
}

void check_session(const MediaCap::Session& session, const char* fallback) {
    if (session.has_error()) throw LibraryError(session.get_error());
    check_error();
    throw std::runtime_error(fallback);
}
}

void record(RecordingOptions& o, Output& out) {
    Signals signals;
    std::optional<OutputPath> path, video_path, audio_path;
    if (!o.capture_only) path.emplace(o.output, o.overwrite);
    if (!o.video_frames.empty()) video_path.emplace(o.video_frames, o.overwrite);
    if (!o.audio_samples.empty()) audio_path.emplace(o.audio_samples, o.overwrite);

    const std::array<OutputPath*, 3> paths{path ? &*path : nullptr, video_path ? &*video_path : nullptr,
        audio_path ? &*audio_path : nullptr};
    require_distinct_paths(paths[0], paths[1]);
    require_distinct_paths(paths[0], paths[2]);
    require_distinct_paths(paths[1], paths[2]);

    FrameSinks frames(paths[1], paths[2]);
    PreparationDeadline preparation(paths, out);
    MediaCap::clear_last_error();
    MediaCap::Session session;
    const auto source_size = add_video(session, o);
    add_audio(session, o.sources);
    const auto audio_callback = !o.sources.empty() && (o.capture_only || audio_path) ? FrameSinks::audio_callback : nullptr;
    const auto video_callback = o.kind != "audio" && (o.capture_only || video_path) ? FrameSinks::video_callback : nullptr;
    if ((audio_callback || video_callback) && !session.set_frame_callbacks(audio_callback, video_callback, &frames))
        check_session(session, "Cannot register raw frame callbacks.");

    auto size = o.output_size.width ? o.output_size : source_size;
    if (!o.capture_only) {
        size.width -= size.width % 2;
        size.height -= size.height % 2;
    }
    MediaCap::VideoEncoderConfig video{size.width, size.height, o.fps, o.video_bitrate, o.video_value};
    MediaCap::AudioEncoderConfig audio{o.sample_rate, static_cast<uint8_t>(o.channels), o.audio_bitrate, o.audio_value};
    const MediaCap::RecordingConfig config{o.output.c_str(), o.container_value, o.kind == "audio" ? nullptr : &video,
        o.sources.empty() ? nullptr : &audio};
    if (interrupted()) {
        out.event("cancelled", "", "Cancelled before capture started.");
        return;
    }
    out.event("preparing", "\"output\":" + quote(o.output), "Preparing capture sources...");
    std::optional<std::chrono::milliseconds> duration;
    if (o.duration > 0) duration = std::chrono::milliseconds(static_cast<int64_t>(o.duration * 1000));
    const bool active = o.capture_only ? session.start_capture() : session.start_recording(config, duration);
    if (!active) check_session(session, o.capture_only ? "Capture did not start." : "Recording did not start.");

    const auto started = std::chrono::steady_clock::now();
    preparation.finish();
    frames.begin_writing();
    auto last_status = started;
    const auto identity = "\"session_id\":" + std::to_string(session.id()) + ",\"output\":" + quote(o.output);
    Input input(o.stdin_control, o.no_interactive);
    out.event("started", identity + ",\"width\":" + std::to_string(size.width) + ",\"height\":" + std::to_string(size.height),
        o.capture_only ? "Capturing frames." : "Recording: " + o.output);
    bool stop = false;
    std::string reason = "completed";
    const auto elapsed = [&] { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); };
    const auto status = [&] {
        out.event("status", identity + ",\"elapsed_seconds\":" + std::to_string(elapsed()) +
            ",\"recording\":" + (session.is_recording() ? "true" : "false") +
            ",\"capturing\":" + (session.is_capturing() ? "true" : "false") +
            ",\"paused\":" + (session.is_paused() ? "true" : "false") + "," + counts_json(frames.counts()),
            std::string(o.capture_only ? "Capturing" : session.is_paused() ? "Paused" : "Recording") +
            "; elapsed " + std::to_string(elapsed()) + " seconds; session " + std::to_string(session.id()));
    };
    while (session.is_capturing() && !stop && !frames.failed() && !session.has_error()) {
        if (interrupted()) { reason = "signal"; break; }
        if (o.capture_only && duration && std::chrono::steady_clock::now() - started >= *duration) break;
        for (const auto& command : input.poll()) {
            if (command == "stop" || command == "quit") { stop = true; reason = "requested"; break; }
            if (command == "status") status();
            else if (command == "pause" || command == "resume" || command == "toggle") {
                if (o.capture_only) {
                    const std::string message = "Pause is available during recording. Use status or stop for raw capture.";
                    out.event("control-error", "\"message\":" + quote(message), message);
                    continue;
                }
                session.set_paused(command == "pause" || (command == "toggle" && !session.is_paused()));
                const bool paused = session.is_paused();
                out.event(paused ? "paused" : "resumed", identity, paused ? "Paused." : "Resumed.");
            } else {
                const auto message = "Unknown control: " + command;
                out.event("control-error", "\"message\":" + quote(message),
                    message + (o.capture_only ? ". Use status or stop." : ". Use pause, resume, toggle, status, or stop."));
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (o.status_interval > 0 && std::chrono::duration<double>(now - last_status).count() >= o.status_interval) {
            status();
            last_status = now;
        }
        if (!stop) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    if (o.capture_only) session.stop_capture();
    else session.stop_recording();
    frames.finish();
    if (session.has_error()) throw LibraryError(session.get_error());
    const auto counts = frames.counts();
    out.event("finished", identity + ",\"reason\":" + quote(reason) + ",\"elapsed_seconds\":" + std::to_string(elapsed()) +
        "," + counts_json(counts), o.capture_only ? counts_text(counts) : "Saved " + o.output + '.');
}
}
