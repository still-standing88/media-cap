#include "source_inventory.hpp"
#include "../../../src/utils/Utf8.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <sstream>
#include <string_view>

namespace mccli {
const char* pixel_name(MediaCap_PixelFormat format) {
    switch (format) {
    case MEDIACAP_PIXEL_FORMAT_BGRA: return "bgra";
    case MEDIACAP_PIXEL_FORMAT_BGR24: return "bgr24";
    case MEDIACAP_PIXEL_FORMAT_RGBA: return "rgba";
    case MEDIACAP_PIXEL_FORMAT_RGB24: return "rgb24";
    case MEDIACAP_PIXEL_FORMAT_NV12: return "nv12";
    case MEDIACAP_PIXEL_FORMAT_I420: return "i420";
    default: return "unknown";
    }
}

namespace {
using Json = DiscoveryJson;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), MediaCap::Utf8::lower_ascii);
    return value;
}

template<class Integer>
std::optional<Integer> number(std::string_view text, int base = 10) {
    if (text.empty()) return std::nullopt;
    Integer value{};
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value, base);
    if (result.ec != std::errc{} || result.ptr != end) return std::nullopt;
    return value;
}

template<class Fetch>
auto fetch(Fetch get, MediaCap::Error& error) {
    MediaCap::clear_last_error();
    auto result = get();
    error = MediaCap::get_last_error();
    return result;
}

template<class Fetch>
auto inventory_items(Listing& list, Fetch get) {
    auto items = fetch(get, list.warning);
    if (items.empty() && list.warning.code != MEDIACAP_ERROR_NONE) throw LibraryError(list.warning);
    return items;
}

bool matches_name(const std::string& name, const ListOptions& options) {
    return options.name.empty() || lower(name).find(lower(options.name)) != std::string::npos;
}

template<class Integer>
bool matches_id(Integer value, const ListOptions& options) {
    return options.id.empty() || number<Integer>(options.id) == value;
}

std::string dimensions(uint32_t width, uint32_t height) {
    return std::to_string(width) + 'x' + std::to_string(height);
}

Listing monitors(const ListOptions& options) {
    Listing list("Monitors", {"ID", "SIZE", "NAME"});
    for (const auto& monitor : inventory_items(list, MediaCap::get_monitors)) {
        if (!matches_id(monitor.id, options) || !matches_name(monitor.name, options)) continue;
        list.rows.push_back({
            {std::to_string(monitor.id), dimensions(monitor.width, monitor.height), monitor.name},
            {{"id", monitor.id}, {"index", monitor.index}, {"name", monitor.name},
             {"width", monitor.width}, {"height", monitor.height}, {"x", monitor.offsetX}, {"y", monitor.offsetY},
             {"scaling", monitor.scaling}},
            "    Position: " + std::to_string(monitor.offsetX) + ',' + std::to_string(monitor.offsetY) +
                "  Scale: " + Json(monitor.scaling).dump()});
    }
    return list;
}

Listing windows(const ListOptions& options) {
    Listing list("Windows", {"HANDLE", "SIZE", "TITLE"});
    for (const auto& window : inventory_items(list, MediaCap::get_windows)) {
        if (!matches_id(window.handle, options) || !matches_name(window.name, options)) continue;
        list.rows.push_back({
            {std::to_string(window.handle), dimensions(window.width, window.height),
             window.name[0] ? window.name : "(untitled)"},
            {{"handle", window.handle}, {"title", window.name}, {"width", window.width}, {"height", window.height},
             {"x", window.offsetX}, {"y", window.offsetY}},
            "    Position: " + std::to_string(window.offsetX) + ',' + std::to_string(window.offsetY)});
    }
    return list;
}

Json camera_data(const MediaCap::Camera& camera) {
    return {{"index", camera.index}, {"name", camera.name}, {"unique_id", camera.unique_id},
            {"details_available", false}, {"resolutions", Json::array()}, {"pixel_formats", Json::array()}};
}

void add_camera_details(Json& data, const MediaCap::CameraDetail& detail) {
    for (const auto& size : detail.supported_resolutions)
        data["resolutions"].push_back({{"width", size.width}, {"height", size.height}});
    for (const auto format : detail.supported_pixel_formats) data["pixel_formats"].push_back(pixel_name(format));
    data["details_available"] = true;
}

std::string camera_details(const Json& data) {
    if (!data["details_available"].get<bool>()) return "    Camera details unavailable.";
    std::ostringstream text;
    text << "    Sizes:";
    for (const auto& size : data["resolutions"])
        text << ' ' << dimensions(size["width"].get<uint32_t>(), size["height"].get<uint32_t>());
    if (data["resolutions"].empty()) text << " unknown";
    text << "\n    Pixel formats:";
    for (const auto& pixel : data["pixel_formats"]) text << ' ' << pixel.get<std::string>();
    if (data["pixel_formats"].empty()) text << " unknown";
    return text.str();
}

Listing cameras(const ListOptions& options, bool verbose) {
    Listing list("Cameras", {"ID", "NAME"});
    for (const auto& camera : inventory_items(list, MediaCap::get_camera_devices)) {
        if (!matches_id(camera.index, options) || !matches_name(camera.name, options)) continue;
        list.rows.push_back({{std::to_string(camera.index), camera.name}, camera_data(camera), {}});
    }
    if (!options.details || list.rows.empty()) return list;

#ifdef _WIN32
    if (!options.worker) {
        const auto query = query_camera_details(options, true, verbose);
        list.diagnostic = query.diagnostic;
        const auto details = Json::parse(query.result, nullptr, false);
        if (details.is_array()) {
            for (auto& row : list.rows) {
                const auto match = std::find_if(details.begin(), details.end(), [&](const Json& item) {
                    return item.is_object() && item.contains("index") && item["index"] == row.data["index"];
                });
                if (match != details.end()) row.data = *match;
            }
        }
        if (query.timed_out || query.status != 0 || !details.is_array()) {
            list.warning.code = query.timed_out ? MEDIACAP_ERROR_TIMEOUT : MEDIACAP_ERROR_DEVICE_UNAVAILABLE;
            list.warning.component = MEDIACAP_ERROR_COMPONENT_DISCOVERY;
            list.warning.source_type = MEDIACAP_SOURCE_TYPE_CAMERA;
        }
    } else
#else
    (void)verbose;
#endif
    {
        MediaCap::Error detail_error{};
        const auto details = fetch(MediaCap::get_cameras, detail_error);
        for (auto& row : list.rows) {
            const auto index = row.data["index"].get<int32_t>();
            const auto match = std::find_if(details.begin(), details.end(), [index](const auto& item) {
                return item.camera.index == index;
            });
            if (match != details.end()) add_camera_details(row.data, *match);
        }
        if (detail_error.code != MEDIACAP_ERROR_NONE) list.warning = detail_error;
    }
    for (auto& row : list.rows) row.detail = camera_details(row.data);
    return list;
}

Listing audio_devices(const std::string& command, const ListOptions& options) {
    const bool inputs = command == "audio-input";
    const bool outputs = command == "audio-output";
    Listing list(inputs ? "Audio inputs" : outputs ? "Audio outputs" : "Audio devices",
                 {inputs || outputs ? "ID" : "SOURCE", "NAME"});
    unsigned input_index = 0;
    unsigned output_index = 0;
    for (const auto& device : inventory_items(list, MediaCap::get_audio_capture_devices)) {
        const bool mic = device.kind == MEDIACAP_AUDIO_CAPTURE_MIC;
        const auto index = mic ? input_index++ : output_index++;
        if ((inputs && !mic) || (outputs && mic) || !matches_name(device.name, options)) continue;
        if (!options.id.empty()) {
            const std::string_view selector = options.id;
            const bool force_opaque = selector.substr(0, 3) == "id:";
            const auto numeric = force_opaque ? std::nullopt : number<unsigned>(selector);
            if (numeric ? *numeric != index : selector.substr(force_opaque ? 3 : 0) != device.id) continue;
        }
        const std::string kind = mic ? "input" : "output";
        const std::string index_text = std::to_string(index);
        list.rows.push_back({
            {inputs || outputs ? index_text : kind + ':' + index_text,
             device.name + (device.is_default ? " (default)" : "")},
            {{"index", index}, {"kind", kind}, {"id", device.id}, {"name", device.name},
             {"sample_rate", device.sample_rate}, {"channels", device.channels}, {"is_default", device.is_default}},
            "    Device: " + single_line(device.id, 180) + "\n    Format: " +
                std::to_string(device.sample_rate) + " Hz, " + std::to_string(device.channels) + " channels"});
    }
    return list;
}

const char* compat_kind(MediaCap_AudioDeviceType value) {
    switch (value) {
    case MEDIACAP_AUDIO_DEVICE_TYPE_INPUT: return "input";
    case MEDIACAP_AUDIO_DEVICE_TYPE_OUTPUT: return "output";
    case MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK: return "loopback";
    default: return "unknown";
    }
}

Listing compat_devices(const ListOptions& options) {
    Listing list("Compatibility audio devices", {"ID", "KIND", "NAME"});
    const auto devices = inventory_items(list, MediaCap::get_audio_devices);
    for (size_t index = 0; index < devices.size(); ++index) {
        const auto& device = devices[index];
        if (!matches_id(index, options) || !matches_name(device.name, options)) continue;
        const char* kind = compat_kind(device.type);
        list.rows.push_back({
            {std::to_string(index), kind, std::string(device.name) + (device.is_default ? " (default)" : "")},
            {{"index", index}, {"kind", kind}, {"name", device.name}, {"is_default", device.is_default != 0}},
            {}});
    }
    return list;
}

Listing processes(const ListOptions& options) {
    Listing list("Audio processes", {"PID", "STATE", "NAME"});
    for (const auto& process : inventory_items(list, MediaCap::get_audio_processes)) {
        if (!matches_id(process.pid, options) || (options.active && process.output_active != 1) ||
            (!matches_name(process.name, options) && !matches_name(process.executable, options))) continue;
        const char* state = process.output_active == 1 ? "playing" : process.output_active == 0 ? "idle" : "unknown";
        list.rows.push_back({
            {std::to_string(process.pid), state, process.name},
            {{"pid", process.pid}, {"name", process.name}, {"executable", process.executable},
             {"output_active", process.output_active}},
            process.executable.empty() ? "" : "    Executable: " + single_line(process.executable)});
    }
    return list;
}

}

Listing inventory(const std::string& category, const ListOptions& options, bool verbose) {
    if (category == "monitors") return monitors(options);
    if (category == "windows") return windows(options);
    if (category == "cameras") return cameras(options, verbose);
    if (category == "audio-classic") return compat_devices(options);
    if (category == "processes") return processes(options);
    if (category == "audio" || category == "audio-input" || category == "audio-output")
        return audio_devices(category, options);
    throw UsageError("Unknown source category.");
}

std::string id_for_category(const std::string& kind, const std::string& id) {
    if (kind == "window") {
        const bool hex = id.compare(0, 2, "0x") == 0 || id.compare(0, 2, "0X") == 0;
        const auto value = number<uint64_t>(std::string_view(id).substr(hex ? 2 : 0), hex ? 16 : 10);
        if (!value || !*value) throw UsageError("Use a decimal or hexadecimal window handle from 'MediaCap list windows'.");
        return std::to_string(*value);
    }
    if (kind == "monitor") {
        const auto value = number<int32_t>(id);
        if (!value) throw UsageError("Use a numeric monitor ID from 'MediaCap list monitors'.");
        return std::to_string(*value);
    }
    if (kind == "camera" || kind == "compat-audio" || kind == "audio-process") {
        const auto value = number<uint32_t>(id);
        if (!value || (kind == "audio-process" && !*value))
            throw UsageError("Use a numeric ID from the corresponding source list.");
        return std::to_string(*value);
    }
    if (id.empty() || id == "id:") throw UsageError("Specify an audio device index or full device ID.");
    return id;
}

}
