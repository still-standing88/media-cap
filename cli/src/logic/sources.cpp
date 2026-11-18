#include "sources.hpp"
#include "../cli.hpp"

#include <algorithm>

namespace mccli {
MediaCap::Monitor select_monitor(std::optional<int> id) {
    MediaCap::clear_last_error();
    const auto monitors = MediaCap::get_monitors();
    check_error();
    const auto selected = id ? std::find_if(monitors.begin(), monitors.end(), [&](const auto& m) { return m.id == *id; }) : monitors.begin();
    if (selected == monitors.end()) throw UsageError("Monitor not found. Run 'MediaCap list monitors'.");
    return *selected;
}

MediaCap::Window select_window(uint64_t handle, const std::string& title) {
    if (!handle && title.empty()) throw UsageError("Select --window HANDLE or --title TITLE.");
    if (handle && !title.empty()) throw UsageError("Use only one of --window and --title.");
    MediaCap::clear_last_error();
    const auto windows = MediaCap::get_windows();
    check_error();
    const auto matches = [&](const auto& w) { return handle ? w.handle == handle : title == w.name; };
    const auto selected = std::find_if(windows.begin(), windows.end(), matches);
    if (selected == windows.end()) throw UsageError("Window not found. Run 'MediaCap list windows'.");
    if (std::find_if(selected + 1, windows.end(), matches) != windows.end())
        throw UsageError("Window title is ambiguous. Select --window HANDLE.");
    return *selected;
}

MediaCap::Camera select_camera(int index) {
    MediaCap::clear_last_error();
    const auto cameras = MediaCap::get_camera_devices();
    check_error();
    const auto selected = std::find_if(cameras.begin(), cameras.end(), [&](const auto& c) { return c.index == index; });
    if (selected == cameras.end()) throw UsageError("Camera not found. Run 'MediaCap list cameras'.");
    return *selected;
}

void add_audio(MediaCap::Session& session, const std::vector<AudioSource>& sources) {
    std::vector<MediaCap::AudioCaptureDevice> devices;
    if (std::any_of(sources.begin(), sources.end(), [](const AudioSource& s) { return s.device_index.has_value(); })) {
        MediaCap::clear_last_error();
        devices = MediaCap::get_audio_capture_devices(); check_error();
    }
    std::vector<MediaCap::AudioDevice> classic;
    if (std::any_of(sources.begin(), sources.end(), [](const AudioSource& s) { return s.kind == "classic"; })) {
        MediaCap::clear_last_error();
        classic = MediaCap::get_audio_devices(); check_error();
    }
    for (const auto& s : sources) {
        MediaCap::clear_last_error();
        if (s.kind == "classic") {
            if (static_cast<size_t>(s.classic_index) >= classic.size()) throw UsageError("Classic audio index not found; run 'MediaCap list audio compat'");
            const auto& device = classic[static_cast<size_t>(s.classic_index)];
            if (device.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT && device.type != MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK) throw UsageError("Classic source must be input or loopback, not playback-only output");
            session.add_source(device, MediaCap_AudioFormat{s.format, s.rate, static_cast<uint8_t>(s.channels)});
            check_error();
        } else {
            auto config = MediaCap::default_audio_source_config();
            config.kind = s.kind == "mic" ? MEDIACAP_AUDIO_CAPTURE_MIC : s.kind == "system" ? MEDIACAP_AUDIO_CAPTURE_SYSTEM : s.kind == "mix" ? MEDIACAP_AUDIO_CAPTURE_MIX : MEDIACAP_AUDIO_CAPTURE_PROCESS;
            config.device_id = s.device.empty() ? nullptr : s.device.c_str();
            if (s.device_index) {
                unsigned index = 0;
                const MediaCap::AudioCaptureDevice* selected = nullptr;
                for (const auto& device : devices) {
                    if (device.kind != config.kind) continue;
                    if (index++ == *s.device_index) { selected = &device; break; }
                }
                if (!selected) throw UsageError(std::string("Audio device ID not found. Run 'MediaCap list audio ") + (s.kind == "mic" ? "input" : "output") + "'.");
                config.device_id = selected->id.c_str();
            }
            config.process_id = s.pid;
            config.process_mode = s.kind == "exclude-process" ? MEDIACAP_PROCESS_EXCLUDE : MEDIACAP_PROCESS_INCLUDE;
            config.exclude_self = s.exclude_self;
            config.exclude_pids = s.exclude_pids.empty() ? nullptr : s.exclude_pids.data();
            config.exclude_pid_count = s.exclude_pids.size();
            config.mix_mic_device_id = s.mic_device.empty() ? nullptr : s.mic_device.c_str();
            config.mix_system_device_id = s.system_device.empty() ? nullptr : s.system_device.c_str();
            config.gain = s.gain; config.mix_mic_gain = s.mic_gain; config.mix_system_gain = s.system_gain;
            config.sample_rate = s.rate; config.channels = static_cast<uint16_t>(s.channels); config.ring_capacity_chunks = s.queue;
            if (!session.add_audio_source(config)) { check_error(); throw std::runtime_error("Could not add audio source: " + s.kind); }
            check_error();
        }
    }
}
}
