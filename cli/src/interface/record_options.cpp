#include "../cli.hpp"
#include "../logic/formats.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace mccli {
static uint64_t integer(const std::string& value, const std::string& field, uint64_t min, uint64_t max) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw UsageError(field + " must be an unsigned decimal integer");
    uint64_t n;
    try { n = std::stoull(value); } catch (...) { throw UsageError(field + " is out of range"); }
    if (n < min || n > max) throw UsageError(field + " is out of range");
    return n;
}
static float gain_value(const std::string& text) {
    size_t used = 0;
    float n;
    try { n = std::stof(text, &used); } catch (...) { throw UsageError("Invalid audio gain: " + text); }
    if (used != text.size() || !std::isfinite(n) || n < 0) throw UsageError("Audio gains must be finite and nonnegative");
    return n;
}
static std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> result;
    size_t start = 0, end;
    do { end = value.find(separator, start); result.push_back(value.substr(start, end - start)); start = end + 1; }
    while (end != std::string::npos);
    return result;
}
static std::string device_id(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        const auto escape = text.substr(i, 3);
        if (escape == "%25") { result += '%'; i += 2; }
        else if (escape == "%3B" || escape == "%3b") { result += ';'; i += 2; }
        else result += text[i];
    }
    return result;
}
static AudioSource parse_audio(const std::string& spec) {
    auto parts = split(spec, ';');
    AudioSource source;
    const auto colon = parts[0].find(':');
    source.kind = parts[0].substr(0, colon);
    if (source.kind == "input" || source.kind == "output") {
        if (colon != std::string::npos)
            source.device_index = static_cast<unsigned>(integer(parts[0].substr(colon + 1), "audio device ID", 0, UINT32_MAX));
        source.kind = source.kind == "input" ? "mic" : "system";
    } else if (source.kind == "compat") source.kind = "classic";
    std::set<std::string> allowed{"gain", "rate", "channels", "queue"};
    if (source.kind == "classic") {
        allowed = {"rate", "channels", "format"};
        source.rate = 48000; source.channels = 2;
        if (colon == std::string::npos) throw UsageError("classic requires :INDEX from list audio-classic");
        source.classic_index = static_cast<int>(integer(parts[0].substr(colon + 1), "classic index", 0, INT32_MAX));
    } else if (source.kind == "process" || source.kind == "exclude-process") {
        if (colon == std::string::npos) throw UsageError("process audio requires :PID");
        source.pid = static_cast<uint32_t>(integer(parts[0].substr(colon + 1), "PID", 1, UINT32_MAX));
    } else {
        if (colon != std::string::npos && !source.device_index) throw UsageError("Use input:ID or output:ID from 'MediaCap list audio'.");
        if (source.kind == "mic" || source.kind == "system") allowed.insert("device");
        else if (source.kind == "mix") allowed.insert({"mic-device", "system-device", "mic-gain", "system-gain"});
        else throw UsageError("Unknown audio source: " + source.kind + "; see MediaCap help audio");
        if (source.kind == "system" || source.kind == "mix") allowed.insert({"exclude-self", "exclude-pids"});
    }
    std::set<std::string> seen;
    for (size_t i = 1; i < parts.size(); ++i) {
        const auto equal = parts[i].find('=');
        const auto key = parts[i].substr(0, equal);
        if (equal == std::string::npos || equal + 1 == parts[i].size() || !allowed.count(key))
            throw UsageError("Invalid/inapplicable audio setting '" + parts[i] + "' for " + source.kind);
        if (!seen.insert(key).second) throw UsageError("Duplicate audio setting: " + key);
        const auto value = parts[i].substr(equal + 1);
        if (key == "device") source.device = device_id(value);
        else if (key == "mic-device") source.mic_device = device_id(value);
        else if (key == "system-device") source.system_device = device_id(value);
        else if (key == "gain") source.gain = gain_value(value);
        else if (key == "mic-gain") source.mic_gain = gain_value(value);
        else if (key == "system-gain") source.system_gain = gain_value(value);
        else if (key == "rate") source.rate = static_cast<uint32_t>(integer(value, "capture rate", source.kind == "classic" ? 1 : 0, 384000));
        else if (key == "channels") source.channels = static_cast<unsigned>(integer(value, "capture channels", source.kind == "classic" ? 1 : 0, source.kind == "classic" ? 32 : 2));
        else if (key == "queue") source.queue = static_cast<size_t>(integer(value, "queue", 0, 1048576));
        else if (key == "exclude-self") {
            if (value != "true" && value != "false") throw UsageError("exclude-self must be true or false");
            source.exclude_self = value == "true";
        } else if (key == "exclude-pids") {
            for (const auto& pid : split(value, ',')) source.exclude_pids.push_back(static_cast<uint32_t>(integer(pid, "excluded PID", 1, UINT32_MAX)));
        } else if (key == "format") {
            const std::map<std::string, MediaCap_SampleFormat> formats{{"u8", MEDIACAP_SAMPLE_FORMAT_U8}, {"s16", MEDIACAP_SAMPLE_FORMAT_S16}, {"s32", MEDIACAP_SAMPLE_FORMAT_S32}, {"f32", MEDIACAP_SAMPLE_FORMAT_FLT}};
            if (!formats.count(value)) throw UsageError("Classic format must be u8, s16, s32 or f32");
            source.format = formats.at(value);
        }
    }
    if (source.device_index && !source.device.empty()) throw UsageError("Choose either a numeric audio ID or device=ID, not both.");
    return source;
}
static MediaCap::Resolution resolution(const std::string& value) {
    auto dimensions = split(value, 'x');
    if (dimensions.size() != 2) throw UsageError("Size must be WIDTHxHEIGHT, e.g. 1920x1080");
    return {static_cast<uint32_t>(integer(dimensions[0], "width", 2, 32768)), static_cast<uint32_t>(integer(dimensions[1], "height", 2, 32768))};
}
template<class T, class Choices> static T choice(const std::string& value, const Choices& values, const char* label) {
    const auto it = std::find(values.begin(), values.end(), value);
    if (it == values.end()) throw UsageError(std::string("Unknown ") + label + ": " + value);
    return static_cast<T>(it - values.begin());
}
void add_record_options(CLI::App& c, RecordingOptions& o) {
    const auto number = [](double minimum, bool inclusive, const char* message) {
        return CLI::Validator([=](std::string& value) -> std::string {
            try {
                size_t used = 0;
                const double n = std::stod(value, &used);
                if (used == value.size() && std::isfinite(n) && (inclusive ? n >= minimum : n > minimum)) return "";
            } catch (...) {}
            return message;
        }, "NUMBER");
    };
    const auto positive = number(0, false, "must be greater than zero");
    const auto nonnegative = number(0, true, "must be zero or greater");
    auto* audio = c.add_option("--audio", o.audio, "Add a source. Repeat to mix sources. See 'MediaCap help audio'")
        ->type_name("SOURCE")->expected(1)->take_all()->group("Audio");
    if (!o.capture_only) {
        c.add_option("-o,--output", o.output, "Recording output file")->type_name("FILE")->required()->group("Output");
        c.add_option("--container", o.container, "auto/mp4/mkv/webm/m4a/flac/mp3/wav/opus")->capture_default_str()->group("Output");
    }
    c.add_flag("--overwrite", o.overwrite, "Replace an existing output file")->group("Output");
    if (o.kind != "audio") c.add_option("--video-frames", o.video_frames, "Write raw video frames and metadata")->type_name("FILE")->group("Output");
    c.add_option("--audio-samples", o.audio_samples, "Write raw audio blocks and metadata")->type_name("FILE")->needs(audio)->group("Output");
    c.add_option("-d,--duration", o.duration, "Stop after this many seconds. The timer starts once the sources are ready")
        ->type_name("SECONDS")->check(number(.001, true, "must be at least 0.001 seconds"))->group("Control");
    c.add_flag("--stdin-control", o.stdin_control, "Read control commands from stdin")->group("Control");
    c.add_flag("--no-interactive", o.no_interactive, "Disable keyboard controls")->group("Control");
    c.add_option("--status-interval", o.status_interval, "Periodic status in seconds; 0 disables")->check(nonnegative)->group("Control");
    c.add_flag("--dry-run", o.dry_run, "Check settings without opening sources or files")->group("Control");
    if (!o.capture_only) {
        c.add_option("--audio-codec", o.audio_codec, "auto/aac/opus/vorbis/flac/mp3/pcm")->capture_default_str()->needs("--audio")->group("Audio");
        c.add_option("--audio-bitrate", o.audio_bitrate, "Output bits/second (0 permitted for lossless)")->capture_default_str()->check(nonnegative)->needs("--audio")->group("Audio");
        c.add_option("--sample-rate", o.sample_rate, "Output sample rate in Hz")->capture_default_str()->check(CLI::Range(1, 384000))->needs("--audio")->group("Audio");
        c.add_option("--channels", o.channels, "Output channel count")->capture_default_str()->check(CLI::Range(1, 32))->needs("--audio")->group("Audio");
    }
    if (!o.capture_only && o.kind != "audio") {
        c.add_option("--size", o.size, "Encoded dimensions. Default is the source size")->type_name("WIDTHxHEIGHT")->group("Video");
        c.add_option("--fps", o.fps, "Output frames/second")->capture_default_str()->check(CLI::Range(1, 240))->group("Video");
        c.add_option("--video-codec", o.video_codec, "auto/h264/vp9/av1")->capture_default_str()->group("Video");
        c.add_option("--video-bitrate", o.video_bitrate, "Output bits/second")->capture_default_str()->check(positive)->group("Video");
    }
    if (o.kind == "screen") c.add_option("--monitor", o.monitor, "Monitor ID (default: first monitor)")->group("Source");
    if (o.kind == "screen" || o.kind == "window")
        c.add_option("--capture-interval", o.capture_interval, "Capture interval in milliseconds")->capture_default_str()->check(CLI::Range(1, 60000))->group("Source");
    if (o.kind == "window") {
        auto* handle = c.add_option("--window", o.window, "Window handle from list windows")->check(positive)->group("Source");
        auto* title = c.add_option("--title", o.window_title, "Exact, unique window title")->group("Source");
        handle->excludes(title);
    }
    if (o.kind == "camera") {
        c.add_option("--camera", o.camera, "Camera index from list cameras")->capture_default_str()->check(nonnegative)->group("Source");
        c.add_option("--capture-size", o.capture_size, "Dimensions requested from the camera")->type_name("WIDTHxHEIGHT")->capture_default_str()->group("Source");
        c.add_option("--capture-fps", o.capture_fps, "Requested camera frames/second")->capture_default_str()->check(CLI::Range(1, 240))->group("Source");
        c.add_option("--pixel-format", o.pixel_format, "bgra/bgr24/rgba/rgb24/nv12/i420")->capture_default_str()->group("Source");
    }
}
void validate(RecordingOptions& o) {
    if (!o.capture_only && o.output.empty()) throw UsageError("--output must not be empty");
    if (o.kind == "window" && !o.window && o.window_title.empty()) throw UsageError("Select a window with --window HANDLE or --title EXACT_TITLE.");
    if (!std::isfinite(o.duration) || o.duration < 0 || o.duration > 31536000 || (o.duration > 0 && o.duration < .001)) throw UsageError("Duration must be between 0.001 and 31536000 seconds");
    if (!std::isfinite(o.status_interval) || o.status_interval < 0) throw UsageError("Status interval must be finite and nonnegative");
    o.container_value = choice<MediaCap_ContainerFormat>(o.container, recording_containers, "container");
    o.video_value = choice<MediaCap_VideoCodec>(o.video_codec, recording_video_codecs, "video codec");
    o.audio_value = choice<MediaCap_AudioCodec>(o.audio_codec, recording_audio_codecs, "audio codec");
    if (!o.size.empty()) {
        o.output_size = resolution(o.size);
        if (o.output_size.width % 2 || o.output_size.height % 2) throw UsageError("Output size must have even width and height");
    }
    if (o.kind == "camera") {
        o.camera_size = resolution(o.capture_size);
        const std::map<std::string, MediaCap_PixelFormat> formats{{"bgra",MEDIACAP_PIXEL_FORMAT_BGRA},{"bgr24",MEDIACAP_PIXEL_FORMAT_BGR24},{"rgba",MEDIACAP_PIXEL_FORMAT_RGBA},{"rgb24",MEDIACAP_PIXEL_FORMAT_RGB24},{"nv12",MEDIACAP_PIXEL_FORMAT_NV12},{"i420",MEDIACAP_PIXEL_FORMAT_I420}};
        if (!formats.count(o.pixel_format)) throw UsageError("Unknown camera pixel format: " + o.pixel_format);
        o.pixel_value = formats.at(o.pixel_format);
    }
    o.sources.clear();
    for (const auto& spec : o.audio) o.sources.push_back(parse_audio(spec));
    if (o.kind == "audio" && o.sources.empty()) throw UsageError("An audio-only session requires at least one --audio SOURCE. See 'MediaCap help audio'.");
    if (o.capture_only) return;
    MediaCap::VideoEncoderConfig video{o.output_size.width, o.output_size.height, o.fps, o.video_bitrate, o.video_value};
    MediaCap::AudioEncoderConfig audio{o.sample_rate, static_cast<uint8_t>(o.channels), o.audio_bitrate, o.audio_value};
    const MediaCap::RecordingConfig config{o.output.c_str(), o.container_value,
        o.kind == "audio" ? nullptr : &video, o.sources.empty() ? nullptr : &audio};
    MediaCap_ClearLastError();
    if (!MediaCap_ValidateRecordingFormat(&config)) {
        MediaCap_Error error{};
        if (MediaCap_GetLastError(&error)) throw UsageError(error.message);
        throw UsageError("Incompatible recording format. See 'MediaCap help codecs'.");
    }
}
std::string plan_json(const RecordingOptions& o) {
    std::ostringstream s;
    s << "\"source\":" << quote(o.kind) << ",\"output\":" << quote(o.output) << ",\"container\":" << quote(o.container)
      << ",\"duration_seconds\":" << o.duration << ",\"monitor_id\":" << (o.monitor ? std::to_string(*o.monitor) : "null") << ",\"window_handle\":" << o.window
      << ",\"window_title\":" << quote(o.window_title) << ",\"camera_index\":" << o.camera
      << ",\"output_size\":" << quote(o.size.empty() ? "source" : o.size) << ",\"fps\":" << o.fps
      << ",\"video_codec\":" << quote(o.video_codec) << ",\"video_bitrate\":" << o.video_bitrate
      << ",\"capture_interval_ms\":" << o.capture_interval << ",\"capture_size\":" << quote(o.capture_size)
      << ",\"capture_fps\":" << o.capture_fps << ",\"pixel_format\":" << quote(o.pixel_format)
      << ",\"audio_codec\":" << quote(o.audio_codec) << ",\"audio_bitrate\":" << o.audio_bitrate
      << ",\"sample_rate\":" << o.sample_rate << ",\"channels\":" << o.channels << ",\"audio_sources\":[";
    for (size_t i = 0; i < o.audio.size(); ++i) { if (i) s << ','; s << quote(o.audio[i]); }
    s << "],\"overwrite\":" << (o.overwrite ? "true" : "false") << ",\"stdin_control\":" << (o.stdin_control ? "true" : "false")
      << ",\"capture_only\":" << (o.capture_only ? "true" : "false")
      << ",\"video_frames\":" << quote(o.video_frames) << ",\"audio_samples\":" << quote(o.audio_samples);
    return s.str();
}
}
