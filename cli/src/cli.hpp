#pragma once
#include <MediaCap.hpp>
#include "CLI11.hpp"
#include <cstdio>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace mccli {
struct UsageError : std::runtime_error { using std::runtime_error::runtime_error; };
struct LibraryError : std::runtime_error {
    MediaCap::Error detail;
    explicit LibraryError(const MediaCap::Error& e) : std::runtime_error(e.message), detail(e) {}
};
void check_error();
std::string quote(const std::string& text);
std::string error_json(const MediaCap::Error& error);
std::string friendly_error(const MediaCap::Error& error);
std::string single_line(const std::string& text, size_t limit = 180);
// Own independent output handles before changing any backend standard streams.
class Output {
    FILE* stream_ = nullptr;
    FILE* errors_ = nullptr;
    std::mutex mutex_;
public:
    bool json;
    bool verbose;
    bool failed = false;
    explicit Output(bool json_mode, bool verbose_mode = false);
    ~Output();
    void line(const std::string& value);
    void error(const std::string& message, const std::string& fields = "");
    void warning(const MediaCap::Error& error);
    void diagnostic(const std::string& detail) noexcept;
    void event(const std::string& type, const std::string& fields, const std::string& human);
};
struct AudioSource {
    std::string kind, device, mic_device, system_device;
    int classic_index = -1;
    std::optional<unsigned> device_index;
    uint32_t pid = 0;
    std::vector<uint32_t> exclude_pids;
    bool exclude_self = false;
    float gain = 1, mic_gain = 1, system_gain = 1;
    uint32_t rate = 0;
    unsigned channels = 0;
    size_t queue = 0;
    MediaCap_SampleFormat format = MEDIACAP_SAMPLE_FORMAT_FLT;
};
struct RecordingOptions {
    std::string kind, output, container = "auto", video_codec = "auto", audio_codec = "auto";
    std::string size, capture_size = "640x480", pixel_format = "bgra", window_title;
    std::string video_frames, audio_samples;
    std::optional<int> monitor;
    int camera = 0, fps = 30, capture_fps = 30, capture_interval = 33;
    uint64_t window = 0;
    int64_t video_bitrate = 8000000, audio_bitrate = 192000;
    uint32_t sample_rate = 48000;
    unsigned channels = 2;
    double duration = 0, status_interval = 0;
    bool overwrite = false, dry_run = false, stdin_control = false, no_interactive = false;
    bool capture_only = false;
    std::vector<std::string> audio;
    std::vector<AudioSource> sources;
    MediaCap::Resolution output_size{}, camera_size{};
    MediaCap_ContainerFormat container_value = MEDIACAP_CONTAINER_DEFAULT;
    MediaCap_VideoCodec video_value = MEDIACAP_VIDEO_CODEC_DEFAULT;
    MediaCap_AudioCodec audio_value = MEDIACAP_AUDIO_CODEC_DEFAULT;
    MediaCap_PixelFormat pixel_value = MEDIACAP_PIXEL_FORMAT_BGRA;
};
void add_record_options(CLI::App& command, RecordingOptions& options);
void validate(RecordingOptions& options);
std::string plan_json(const RecordingOptions& options);
void record(RecordingOptions& options, Output& output);
struct ScreenshotOptions {
    std::string kind, output, region, size, window_title;
    std::optional<int> monitor;
    uint64_t window = 0;
    int timeout = 0;
    bool overwrite = false;
};
void screenshot(const ScreenshotOptions& options, Output& output);
struct ListOptions {
    std::string name;
    bool details = false, active = false, worker = false;
    std::string id;
};
struct DiscoveryQuery {
    std::string result, diagnostic;
    int status = 1;
    bool timed_out = false;
};
#ifdef _WIN32
DiscoveryQuery query_camera_details(const ListOptions&, bool json, bool verbose);
#endif
void discover(const std::string& command, Output& output, const ListOptions& options = {});
void source_info(const std::string& kind, const std::string& id, Output& output);
void formats(Output& output);
void requirements(Output& output);
const char* pixel_name(MediaCap_PixelFormat format);
}
