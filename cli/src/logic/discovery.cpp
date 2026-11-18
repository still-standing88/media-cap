#include "source_inventory.hpp"
#include "../../../src/utils/Utf8.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>

namespace mccli {
namespace {
using Json = DiscoveryJson;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), MediaCap::Utf8::lower_ascii);
    return value;
}

std::string json_text(const Json& value) {
    return value.dump(-1, ' ', false, Json::error_handler_t::replace);
}

const char* availability(MediaCap_AudioAvailability value) {
    switch (value) {
    case MEDIACAP_AUDIO_AVAILABLE: return "available";
    case MEDIACAP_AUDIO_UNAVAILABLE: return "unavailable";
    default: return "unknown";
    }
}

const char* scope(MediaCap_AudioPidScope value) {
    switch (value) {
    case MEDIACAP_PID_SCOPE_EXACT: return "exact";
    case MEDIACAP_PID_SCOPE_TREE: return "tree";
    default: return "none";
    }
}

Json error_detail(const MediaCap::Error& error) {
    return {{"code", error.code}, {"component", error.component}, {"source_type", error.source_type},
            {"source_index", error.source_index}, {"session_id", error.session_id},
            {"native_code", error.native_code}, {"message", friendly_error(error)}};
}

void list_all(Output& out, const ListOptions& options) {
    Json result = Json::object();
    Json failures = Json::array();
    Json errors = Json::object();
    const std::vector<std::string> groups{"monitors", "windows", "cameras", "audio-input", "audio-output", "processes"};
    for (const auto& group : groups) {
        try {
            auto list = inventory(group, options, out.verbose);
            if (out.json) result[group] = list.array();
            else out.line(list.table(options));
            if (list.warning.code != MEDIACAP_ERROR_NONE) {
                out.warning(list.warning);
                failures.push_back(group);
                errors[group] = error_detail(list.warning);
            }
            out.diagnostic(list.diagnostic);
        } catch (const LibraryError& error) {
            out.failed = true;
            failures.push_back(group);
            errors[group] = error_detail(error.detail);
            if (out.json) result[group] = Json::array();
            else out.error(group + ": " + friendly_error(error.detail));
            out.diagnostic(error.what());
        }
    }
    if (out.json) {
        result["failed_lists"] = std::move(failures);
        if (!errors.empty()) result["errors"] = std::move(errors);
        out.line(json_text(result));
    }
}

void audio_capabilities(Output& out, bool details) {
    MediaCap::clear_last_error();
    const auto caps = MediaCap::get_audio_capabilities();
    check_error();
    if (out.json) {
        const Json result = {{"backend_enabled", caps.backend_enabled != 0},
            {"microphone", availability(caps.microphone)}, {"system", availability(caps.system)},
            {"process", availability(caps.process)}, {"mix", availability(caps.mix)},
            {"system_exclusions", availability(caps.system_exclusions)}, {"process_scope", scope(caps.process_scope)},
            {"exclusion_scope", scope(caps.exclusion_scope)}, {"exclusion_honors_device", caps.exclusion_honors_device != 0},
            {"max_excluded_pids", caps.max_excluded_pids}, {"process_detail", caps.process_detail},
            {"system_detail", caps.system_detail}};
        out.line(json_text(result));
        return;
    }
    std::ostringstream text;
    text << "Audio capture\n  Input: " << availability(caps.microphone) << "\n  Output: " << availability(caps.system)
         << "\n  Process: " << availability(caps.process) << "\n  Mix: " << availability(caps.mix)
         << "\n  Process exclusion: " << availability(caps.system_exclusions);
    if (details) {
        text << "\n  Process scope: " << scope(caps.process_scope) << "\n  Exclusion scope: " << scope(caps.exclusion_scope)
             << "\n  Excluded PID limit: " << caps.max_excluded_pids
             << "\n  Exclusions keep selected device: " << (caps.exclusion_honors_device ? "yes" : "no");
        if (caps.process_detail[0]) text << "\n  Process detail: " << single_line(caps.process_detail);
        if (caps.system_detail[0]) text << "\n  System detail: " << single_line(caps.system_detail);
    }
    out.line(text.str());
}

std::string named_fields(const Json& data) {
    std::ostringstream text;
    for (auto it = data.begin(); it != data.end(); ++it) {
        if (it.value().is_array() || it.key() == "details_available") continue;
        std::string label = it.key();
        std::replace(label.begin(), label.end(), '_', ' ');
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
        text << label << ": ";
        if (it.value().is_string()) {
            const auto value = it.value().get<std::string>();
            text << (value.empty() ? "(unknown)" : single_line(value, 512));
        } else if (it.key() == "output_active") {
            const auto value = it.value().get<int>();
            text << (value > 0 ? "playing" : value == 0 ? "idle" : "unknown");
        } else text << it.value().dump();
        text << '\n';
    }
    auto result = text.str();
    result.pop_back();
    return result;
}
}

DiscoveryJson Listing::array() const {
    DiscoveryJson result = DiscoveryJson::array();
    for (const auto& row : rows) result.push_back(row.data);
    return result;
}

std::string Listing::table(const ListOptions& options) const {
    std::ostringstream text;
    text << title << " (" << rows.size() << ")\n";
    if (rows.empty()) {
        text << "No " << lowercase(title);
        if (!options.name.empty()) text << " matching " << quote(single_line(options.name, 60));
        else text << " found";
        return text.str() + '.';
    }

    std::vector<size_t> widths;
    widths.reserve(headers.size());
    for (const auto& column : headers) widths.push_back(column.size());
    for (const auto& row : rows) {
        for (size_t i = 0; i + 1 < headers.size(); ++i)
            widths[i] = std::max(widths[i], single_line(row.cells[i], 25).size());
    }
    auto print = [&](const auto& cells) {
        for (size_t i = 0; i < cells.size(); ++i) {
            const bool last = i + 1 == cells.size();
            text << std::left << std::setw(last ? 0 : static_cast<int>(widths[i] + 2))
                 << single_line(cells[i], last ? 60 : 25);
        }
        text << '\n';
    };
    print(headers);
    for (const auto& row : rows) {
        print(row.cells);
        if (options.details && !row.detail.empty()) text << row.detail << '\n';
    }
    auto result = text.str();
    result.pop_back();
    return result;
}

void requirements(Output& out) {
    static constexpr const char* feature_names[]{"screen", "window", "camera", "microphone", "system_audio",
        "process_audio", "mix_audio", "audio_exclusion", "classic_audio"};
    static constexpr std::pair<MediaCap_Prerequisite, const char*> prerequisites[]{
        {MEDIACAP_REQUIRES_DISPLAY_SESSION, "display_session"}, {MEDIACAP_REQUIRES_X11, "x11"},
        {MEDIACAP_REQUIRES_CAMERA_DEVICE, "camera_device"}, {MEDIACAP_REQUIRES_AUDIO_INPUT, "audio_input"},
        {MEDIACAP_REQUIRES_AUDIO_OUTPUT, "audio_output"}, {MEDIACAP_REQUIRES_PIPEWIRE, "pipewire"},
        {MEDIACAP_REQUIRES_SCREEN_PERMISSION, "screen_permission"}, {MEDIACAP_REQUIRES_CAMERA_PERMISSION, "camera_permission"},
        {MEDIACAP_REQUIRES_MICROPHONE_PERMISSION, "microphone_permission"},
        {MEDIACAP_REQUIRES_SYSTEM_AUDIO_PERMISSION, "system_audio_permission"},
        {MEDIACAP_REQUIRES_PROCESS_ID, "process_id"}, {MEDIACAP_REQUIRES_FLEXAUDIO, "flexaudio"},
    };
    static constexpr const char* platform_names[]{"unknown", "windows", "macos", "linux"};
    static_assert(sizeof(feature_names) / sizeof(*feature_names) == MEDIACAP_FEATURE_CLASSIC_AUDIO + 1);
    Json features = Json::object();
    std::ostringstream text;
    const char* platform = "unknown";
    for (unsigned index = 0; index <= MEDIACAP_FEATURE_CLASSIC_AUDIO; ++index) {
        const auto info = MediaCap::get_feature_requirements(static_cast<MediaCap_Feature>(index));
        if (!info) { check_error(); throw std::runtime_error("Could not read platform requirements"); }
        const auto platform_index = static_cast<unsigned>(info->platform);
        if (platform_index < sizeof(platform_names) / sizeof(*platform_names))
            platform = platform_names[platform_index];
        Json required = Json::array();
        for (const auto& flag : prerequisites)
            if (info->prerequisites & flag.first) required.push_back(flag.second);
        Json minimum_os = nullptr;
        text << "\n  " << feature_names[index] << " (" << info->backend << ")";
        if (!info->included_in_build) text << " [omitted from build]";
        if (info->minimum_os_major) {
            minimum_os = {{"major", info->minimum_os_major}, {"minor", info->minimum_os_minor},
                {"build", info->minimum_os_build}};
            text << " requires OS " << info->minimum_os_major << '.' << info->minimum_os_minor;
            if (info->minimum_os_build) text << " build " << info->minimum_os_build;
        }
        if (!required.empty()) {
            text << " [";
            for (size_t i = 0; i < required.size(); ++i)
                text << (i ? ", " : "") << required[i].get<std::string>();
            text << ']';
        }
        features[feature_names[index]] = {{"included_in_build", info->included_in_build != 0},
            {"backend", info->backend}, {"minimum_os", minimum_os}, {"prerequisites", required}};
    }
    out.line(out.json ? json_text(Json{{"platform", platform}, {"features", features}})
        : "Capture requirements for " + std::string(platform) + text.str());
}

void discover(const std::string& command, Output& out, const ListOptions& options) {
    if (command == "all") {
        list_all(out, options);
        return;
    }
    if (command == "capabilities") {
        audio_capabilities(out, options.details);
        return;
    }
    const auto list = inventory(command, options, out.verbose);
    out.line(out.json ? json_text(list.array()) : list.table(options));
    if (list.warning.code != MEDIACAP_ERROR_NONE) out.warning(list.warning);
    out.diagnostic(list.diagnostic);
}

void source_info(const std::string& kind, const std::string& id, Output& out) {
    static const std::map<std::string, std::string> categories{
        {"monitor", "monitors"}, {"window", "windows"}, {"camera", "cameras"},
        {"audio-input", "audio-input"}, {"audio-output", "audio-output"},
        {"audio-process", "processes"}, {"compat-audio", "audio-classic"}};
    const auto category = categories.find(kind);
    if (category == categories.end()) throw UsageError("Unknown source kind.");
    ListOptions options;
    options.id = id_for_category(kind, id);
    options.details = true;
    const auto list = inventory(category->second, options, out.verbose);
    if (list.rows.empty()) {
        if (list.warning.code != MEDIACAP_ERROR_NONE) throw LibraryError(list.warning);
        throw UsageError("Source ID not found. Refresh the corresponding list and try again.");
    }
    if (list.rows.size() != 1) throw UsageError("Source ID matches more than one device. Use a unique ID.");
    const auto& row = list.rows.front();
    if (out.json) {
        auto data = row.data;
        if (list.warning.code != MEDIACAP_ERROR_NONE) data["warning"] = error_detail(list.warning);
        out.line(json_text(data));
    } else {
        auto text = named_fields(row.data);
        if (kind == "camera") text += '\n' + row.detail;
        out.line(text);
    }
    if (list.warning.code != MEDIACAP_ERROR_NONE) out.warning(list.warning);
    out.diagnostic(list.diagnostic);
}
}
