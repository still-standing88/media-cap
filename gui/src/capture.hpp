#pragma once

#include "settings.hpp"
#include "MediaCap.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <variant>

namespace audiopix {

using Device = std::variant<MediaCap_Camera, MediaCap_Monitor, MediaCap_Window,
                            MediaCap_AudioDevice, MediaCap::AudioCaptureDevice, MediaCap::AudioProcess>;
struct Candidate {
    std::string id;
    std::string name;
    Device device;
};

std::vector<Candidate> discover(Kind kind);
bool supports_gain(const Candidate& candidate);

struct PreviewImage {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<uint8_t> rgba;
};

class Capture {
public:
    void start_recording(const std::vector<Source>& sources, const Settings& settings);
    void start_preview(const Source& source);
    std::string finish_recording(bool cancel);
    void stop_preview();
    bool running() const;
    bool paused() const;
    bool toggle_pause();
    std::string failure() const;
    PreviewImage take_image();
    float take_level();

private:
    static void preview_video(size_t, MediaCap_SourceType, const MediaCap_VideoFrameView*, void*);
    static void preview_audio(size_t, MediaCap_SourceType, const MediaCap_AudioFrameView*, void*);

    std::mutex frame_mutex;
    PreviewImage frame;
    std::atomic<float> level{0};
    std::filesystem::path recording_path;
    std::unique_ptr<MediaCap::Session> active; // Destroy first so callbacks have finished.
};

} // namespace audiopix
