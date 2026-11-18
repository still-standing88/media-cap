#include "settings.hpp"
#include "third_party/config_parser/ConfigParser.hpp"

#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace audiopix {
namespace fs = std::filesystem;
namespace {
using ConfigParser::ConfigSection;

fs::path environment_path(const char* name) {
#ifdef _WIN32
    const std::wstring key(name, name + std::char_traits<char>::length(name));
    const auto length = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (!length) return {};
    std::wstring value(length, L'\0');
    const auto copied = GetEnvironmentVariableW(key.c_str(), value.data(), length);
    if (!copied || copied >= length) return {};
    value.resize(copied);
    return value;
#else
    const auto* value = std::getenv(name);
    return value ? fs::path(value) : fs::path{};
#endif
}

fs::path home_path() {
#ifdef _WIN32
    auto home = environment_path("USERPROFILE");
#else
    auto home = environment_path("HOME");
#endif
    return home.empty() ? fs::current_path() : home;
}

std::string encode(const std::string& text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char value : text) {
        if (value <= 32 || value == '%' || value == '=' || value == 127) {
            result += '%';
            result += hex[value >> 4];
            result += hex[value & 15];
        } else {
            result += static_cast<char>(value);
        }
    }
    return result;
}

std::string decode(std::string_view text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && text.size() - i >= 3) {
            unsigned value = 0;
            const auto end = text.data() + i + 3;
            const auto parsed = std::from_chars(text.data() + i + 1, end, value, 16);
            if (parsed.ec == std::errc{} && parsed.ptr == end && value != 0) {
                result += static_cast<char>(value);
                i += 2;
                continue;
            }
        }
        result += text[i];
    }
    return result;
}

std::string value(ConfigSection& section, const char* key, std::string fallback = {}) {
    if (!section.exists(key)) return fallback;
    return decode(static_cast<std::string>(std::as_const(section[key])));
}

int number(ConfigSection& section, const char* key, int fallback, int low, int high) {
    const auto text = value(section, key);
    int result = 0;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end && result >= low && result <= high
        ? result : fallback;
}

constexpr std::array shortcut_keys{"start", "pause_resume", "stop"};
constexpr int max_entries = 10000;

struct NumericSetting {
    const char* key;
    int Settings::* field;
    int low;
    int high;
};

constexpr NumericSetting audio_fields[] = {
    {"rate", &Settings::rate, 8000, 192000}, {"channels", &Settings::channels, 1, 2},
    {"kbps", &Settings::audio_kbps, 8, 1536}, {"codec", &Settings::audio_codec, 0, 6}
};

constexpr NumericSetting video_fields[] = {
    {"width", &Settings::width, 2, 7680}, {"height", &Settings::height, 2, 4320},
    {"fps", &Settings::fps, 1, 120}, {"kbps", &Settings::video_kbps, 100, 100000},
    {"codec", &Settings::video_codec, 0, 3}
};
}

std::string utf8_path(const fs::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

fs::path config_path() {
#ifdef _WIN32
    auto directory = environment_path("APPDATA");
    if (directory.empty()) directory = home_path();
#elif defined(__APPLE__)
    auto directory = home_path() / "Library" / "Application Support";
#else
    auto directory = environment_path("XDG_CONFIG_HOME");
    if (!directory.is_absolute()) directory = home_path() / ".config";
#endif
    return directory / "AudioPix" / "settings.cfg";
}

Settings load_settings(const fs::path& path) {
    Settings settings;
    const auto videos = home_path() / "Videos";
    settings.output = utf8_path(fs::is_directory(videos) ? videos / "AudioPix" : home_path() / "AudioPix");
    if (path.empty() || !fs::exists(path)) return settings;

    ConfigParser::CfgParser cfg(utf8_path(path));
    if (cfg.getError() != ConfigParser::ConfigError::NONE)
        throw std::runtime_error("Could not read the AudioPix configuration.");
    const auto has = [&](const std::string& key) {
        return std::ranges::find(cfg.sections(), key) != cfg.sections().end();
    };
    const auto load_fields = [&](const char* name, const auto& fields) {
        if (!has(name)) return;
        for (const auto& field : fields)
            settings.*field.field = number(cfg[name], field.key, settings.*field.field, field.low, field.high);
    };
    load_fields("audio", audio_fields);
    load_fields("video", video_fields);
    if (has("shortcuts")) {
        for (size_t i = 0; i < shortcut_keys.size(); ++i)
            settings.shortcuts[i] = value(cfg["shortcuts"], shortcut_keys[i]);
    }
    if (!has("preferences")) return settings;
    auto& preferences = cfg["preferences"];
    settings.output = value(preferences, "output", settings.output);
    settings.close_to_tray = value(preferences, "close_to_tray") == "true";
    const auto count = number(preferences, "sessions", 0, 0, max_entries);
    settings.sessions.clear();
    for (int i = 0; i < count; ++i) {
        const auto key = "session." + std::to_string(i);
        if (!has(key)) continue;
        auto& section = cfg[key];
        auto& session = settings.sessions.emplace_back();
        session.name = value(section, "name", session.name);
        const auto count_sources = number(section, "sources", 0, 0, max_entries);
        for (int j = 0; j < count_sources; ++j) {
            const auto source_key = key + ".source." + std::to_string(j);
            if (!has(source_key)) continue;
            auto& data = cfg[source_key];
            const int kind = number(data, "kind", -1, 0, static_cast<int>(kind_names.size()) - 1);
            const auto id = value(data, "id");
            if (kind < 0 || id.empty()) continue;
            Source source;
            source.kind = static_cast<Kind>(kind);
            source.id = id;
            source.name = value(data, "name", id);
            source.fps = number(data, "fps", source.fps, 1, 120);
            source.width = number(data, "width", source.width, 2, 7680);
            source.height = number(data, "height", source.height, 2, 4320);
            source.gain = number(data, "gain", source.gain, 0, 300);
            session.sources.push_back(std::move(source));
        }
    }
    if (settings.sessions.empty()) settings.sessions.emplace_back();
    return settings;
}

void save_settings(const fs::path& path, const Settings& settings) {
    ConfigParser::CfgParser cfg;
    cfg.addSection("preferences");
    auto& preferences = cfg["preferences"];
    preferences["output"] = encode(settings.output);
    preferences["close_to_tray"] = settings.close_to_tray;
    preferences["sessions"] = static_cast<int>(settings.sessions.size());

    const auto save_fields = [&](const char* name, const auto& fields) {
        cfg.addSection(name);
        for (const auto& field : fields)
            cfg[name][field.key] = settings.*field.field;
    };
    save_fields("audio", audio_fields);
    save_fields("video", video_fields);
    cfg.addSection("shortcuts");
    for (size_t i = 0; i < shortcut_keys.size(); ++i)
        cfg["shortcuts"][shortcut_keys[i]] = encode(settings.shortcuts[i]);
    for (size_t i = 0; i < settings.sessions.size(); ++i) {
        const auto& session = settings.sessions[i];
        const auto key = "session." + std::to_string(i);
        cfg.addSection(key);
        cfg[key]["name"] = encode(session.name);
        cfg[key]["sources"] = static_cast<int>(session.sources.size());
        for (size_t j = 0; j < session.sources.size(); ++j) {
            const auto& source = session.sources[j];
            const auto name = key + ".source." + std::to_string(j);
            cfg.addSection(name);
            auto& data = cfg[name];
            data["kind"] = static_cast<int>(source.kind);
            data["id"] = encode(source.id);
            data["name"] = encode(source.name);
            data["fps"] = source.fps;
            data["width"] = source.width;
            data["height"] = source.height;
            data["gain"] = source.gain;
        }
    }
    fs::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    cfg.save(utf8_path(temporary));
    if (cfg.getError() != ConfigParser::ConfigError::NONE)
        throw std::runtime_error("Could not write the AudioPix configuration.");
    fs::rename(temporary, path);
}

} // namespace audiopix
