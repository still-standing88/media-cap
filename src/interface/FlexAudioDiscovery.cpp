#include "utils/ErrorReporter.h"
#include "MediaCap.hpp"
#include "integrations/flexaudio_bridge/bridge.h"
#include <cstring>
#include <stdexcept>
namespace MediaCap {
void report_flex_error(const Errors::Reporter &, const McFlexError &);
namespace {
Errors::Reporter discovery(MEDIACAP_ERROR_COMPONENT_DISCOVERY);
void disabled() {
    discovery.report(MEDIACAP_ERROR_UNSUPPORTED, "flexaudio capture was disabled when MediaCap was built");
}
bool devices(std::vector<AudioCaptureDevice> &result) {
#if MEDIACAP_HAS_FLEXAUDIO
    void *raw = nullptr;
    size_t count = 0;
    McFlexError error{};
    if (mc_flex_devices(&raw, &count, &error) < 0) {
        report_flex_error(discovery, error);
        return false;
    }
    std::unique_ptr<void, decltype(&mc_flex_devices_free)> list(raw, mc_flex_devices_free);
    for (size_t i = 0; i < count; ++i) {
        McFlexDevice item{};
        if (mc_flex_device_at(raw, i, &item) < 0)
            throw std::runtime_error("Invalid flexaudio inventory item");
        result.push_back({item.id, item.name, MediaCap_AudioCaptureKind(item.kind), item.sample_rate,
                          item.channels, item.is_default != 0});
    }
    return true;
#else
    disabled();
    return false;
#endif
}
bool processes(std::vector<AudioProcess> &result) {
#if MEDIACAP_HAS_FLEXAUDIO
    void *raw = nullptr;
    size_t count = 0;
    McFlexError error{};
    if (mc_flex_processes(&raw, &count, &error) < 0) {
        report_flex_error(discovery, error);
        return false;
    }
    std::unique_ptr<void, decltype(&mc_flex_processes_free)> list(raw, mc_flex_processes_free);
    for (size_t i = 0; i < count; ++i) {
        McFlexProcess item{};
        if (mc_flex_process_at(raw, i, &item) < 0)
            throw std::runtime_error("Invalid flexaudio process item");
        result.push_back({item.pid, item.name, item.executable ? item.executable : "", item.output_active});
    }
    return true;
#else
    disabled();
    return false;
#endif
}
char *copy(const std::string &s) {
    auto p = std::make_unique<char[]>(s.size() + 1);
    std::memcpy(p.get(), s.c_str(), s.size() + 1);
    return p.release();
}
}
}
extern "C" MEDIACAP_API int MediaCap_GetFeatureRequirements(MediaCap_Feature feature, MediaCap_FeatureRequirements* out) {
    if (out) *out = {};
    if (!out || static_cast<unsigned>(feature) > MEDIACAP_FEATURE_CLASSIC_AUDIO) {
        MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API)
            .report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid feature requirements query");
        return 0;
    }

    struct Row {
        const char* backend;
        uint32_t prerequisites;
        bool gated;
    };
    constexpr uint32_t screen = MEDIACAP_REQUIRES_DISPLAY_SESSION;
    constexpr uint32_t camera = MEDIACAP_REQUIRES_CAMERA_DEVICE;
    constexpr uint32_t input = MEDIACAP_REQUIRES_AUDIO_INPUT;
    constexpr uint32_t pid = MEDIACAP_REQUIRES_PROCESS_ID;
#if defined(_WIN32)
    out->platform = MEDIACAP_PLATFORM_WINDOWS;
    constexpr uint32_t output = MEDIACAP_REQUIRES_AUDIO_OUTPUT;
    constexpr uint32_t mic = MEDIACAP_REQUIRES_MICROPHONE_PERMISSION;
    constexpr Row requirements[]{
        {"dxgi_or_gdi", screen, false},
        {"dxgi_or_gdi", screen, false},
        {"directshow_or_media_foundation", camera | MEDIACAP_REQUIRES_CAMERA_PERMISSION, false},
        {"cpal", input | mic, false},
        {"wasapi_loopback", output, false},
        {"wasapi_process_loopback", pid, true},
        {"cpal_and_wasapi_loopback", input | output | mic, false},
        {"wasapi_process_loopback", 0, true},
        {"miniaudio", 0, false},
    };
#elif defined(__APPLE__)
    out->platform = MEDIACAP_PLATFORM_MACOS;
    constexpr uint32_t tap = MEDIACAP_REQUIRES_SYSTEM_AUDIO_PERMISSION;
    constexpr uint32_t mic = MEDIACAP_REQUIRES_MICROPHONE_PERMISSION;
    constexpr Row requirements[]{
        {"core_graphics", screen | MEDIACAP_REQUIRES_SCREEN_PERMISSION, false},
        {"core_graphics", screen | MEDIACAP_REQUIRES_SCREEN_PERMISSION, false},
        {"avfoundation", camera | MEDIACAP_REQUIRES_CAMERA_PERMISSION, false},
        {"cpal", input | mic, false},
        {"core_audio_process_tap", tap, true},
        {"core_audio_process_tap", tap | pid, true},
        {"cpal_and_core_audio_tap", input | mic | tap, true},
        {"core_audio_process_tap", tap, true},
        {"miniaudio", 0, false},
    };
#elif defined(__linux__)
    out->platform = MEDIACAP_PLATFORM_LINUX;
    constexpr uint32_t pipewire = MEDIACAP_REQUIRES_PIPEWIRE;
    constexpr Row requirements[]{
        {"x11", screen | MEDIACAP_REQUIRES_X11, false},
        {"x11", screen | MEDIACAP_REQUIRES_X11, false},
        {"v4l2", camera, false},
        {"cpal_alsa", input, false},
        {"pipewire", pipewire, false},
        {"pipewire", pipewire | pid, false},
        {"cpal_and_pipewire", input | pipewire, false},
        {"pipewire", pipewire, false},
        {"miniaudio", 0, false},
    };
#else
    return 1;
#endif
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    static_assert(sizeof(requirements) / sizeof(*requirements) == MEDIACAP_FEATURE_CLASSIC_AUDIO + 1);
    const auto& row = requirements[feature];
    const bool flexaudio = feature >= MEDIACAP_FEATURE_MICROPHONE && feature <= MEDIACAP_FEATURE_AUDIO_EXCLUSION;
    out->included_in_build = !flexaudio || MEDIACAP_HAS_FLEXAUDIO;
    out->prerequisites = row.prerequisites | (flexaudio ? MEDIACAP_REQUIRES_FLEXAUDIO : 0);
    MediaCap::Utf8::copy(out->backend, row.backend);
#if MEDIACAP_HAS_FLEXAUDIO
    if (row.gated)
        mc_flex_minimum_gated_os(&out->minimum_os_major, &out->minimum_os_minor, &out->minimum_os_build);
#endif
    return 1;
#endif
}

extern "C" MEDIACAP_API MediaCap_AudioSourceConfig MediaCap_DefaultAudioSourceConfig() {
    MediaCap_AudioSourceConfig config{};
    config.gain = config.mix_mic_gain = config.mix_system_gain = 1;
    return config;
}
extern "C" MEDIACAP_API int MediaCap_GetAudioCapabilities(MediaCap_AudioCapabilities *out) {
    if (!out) {
        MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API)
            .report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Missing capability output");
        return 0;
    }
    *out = {};
#if MEDIACAP_HAS_FLEXAUDIO
    McFlexError error{};
    if (mc_flex_capabilities(out, &error) < 0) {
        MediaCap::report_flex_error(MediaCap::discovery, error);
        return 0;
    }
#else
    std::snprintf(out->system_detail, sizeof(out->system_detail), "flexaudio disabled at build time");
    std::snprintf(out->process_detail, sizeof(out->process_detail), "flexaudio disabled at build time");
#endif
    return 1;
}
extern "C" MEDIACAP_API void MediaCap_FreeAudioCaptureDevices(MediaCap_AudioCaptureDevice *p, size_t count) {
    if (!p)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] p[i].id;
        delete[] p[i].name;
    }
    delete[] p;
}
extern "C" MEDIACAP_API void MediaCap_FreeAudioProcesses(MediaCap_AudioProcess *p, size_t count) {
    if (!p)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] p[i].name;
        delete[] p[i].executable;
    }
    delete[] p;
}
extern "C" MEDIACAP_API int MediaCap_GetAudioCaptureDevices(MediaCap_AudioCaptureDevice **out,
                                                            size_t *count) {
    if (out)
        *out = nullptr;
    if (count)
        *count = 0;
    if (!out || !count) {
        MediaCap::discovery.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Missing inventory outputs");
        return 0;
    }
    MediaCap_AudioCaptureDevice *result = nullptr;
    size_t n = 0;
    try {
        std::vector<MediaCap::AudioCaptureDevice> source;
        if (!MediaCap::devices(source))
            return 0;
        n = source.size();
        result = new MediaCap_AudioCaptureDevice[n]{};
        for (size_t i = 0; i < n; ++i) {
            const auto &d = source[i];
            result[i].id = MediaCap::copy(d.id);
            result[i].name = MediaCap::copy(d.name);
            result[i].kind = d.kind;
            result[i].sample_rate = d.sample_rate;
            result[i].channels = d.channels;
            result[i].is_default = d.is_default;
        }
        *out = result;
        *count = n;
        return 1;
    } catch (...) {
        MediaCap_FreeAudioCaptureDevices(result, n);
        MediaCap::discovery.exception("Audio device inventory failed");
        return 0;
    }
}
extern "C" MEDIACAP_API int MediaCap_GetAudioProcesses(MediaCap_AudioProcess **out, size_t *count) {
    if (out)
        *out = nullptr;
    if (count)
        *count = 0;
    if (!out || !count) {
        MediaCap::discovery.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Missing process outputs");
        return 0;
    }
    MediaCap_AudioProcess *result = nullptr;
    size_t n = 0;
    try {
        std::vector<MediaCap::AudioProcess> source;
        if (!MediaCap::processes(source))
            return 0;
        n = source.size();
        result = new MediaCap_AudioProcess[n]{};
        for (size_t i = 0; i < n; ++i) {
            const auto &p = source[i];
            result[i].pid = p.pid;
            result[i].name = MediaCap::copy(p.name);
            result[i].executable = p.executable.empty() ? nullptr : MediaCap::copy(p.executable);
            result[i].output_active = p.output_active;
        }
        *out = result;
        *count = n;
        return 1;
    } catch (...) {
        MediaCap_FreeAudioProcesses(result, n);
        MediaCap::discovery.exception("Audio process inventory failed");
        return 0;
    }
}
