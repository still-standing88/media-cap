#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace audiopix {

enum class Kind { Camera, Monitor, Input, Loopback, Window, Process };
inline constexpr std::array kind_names{"Camera", "Monitor", "Input", "Output loopback", "Window", "Process audio"};

inline bool video_source(Kind kind) {
    return kind == Kind::Camera || kind == Kind::Monitor || kind == Kind::Window;
}

struct Source {
    Kind kind = Kind::Camera;
    std::string id;
    std::string name;
    int fps = 30;
    int width = 1280;
    int height = 720;
    int gain = 100;
};

struct SavedSession {
    std::string name = "Untitled";
    std::vector<Source> sources;
};

struct Settings {
    std::vector<SavedSession> sessions{{}};
    std::string output;
    bool close_to_tray = false;
    std::array<std::string, 3> shortcuts;
    int rate = 48000;
    int channels = 2;
    int audio_kbps = 192;
    int audio_codec = 0;
    int width = 1280;
    int height = 720;
    int fps = 30;
    int video_kbps = 4000;
    int video_codec = 0;
};

std::string utf8_path(const std::filesystem::path& path);
std::filesystem::path config_path();
Settings load_settings(const std::filesystem::path& path);
void save_settings(const std::filesystem::path& path, const Settings& settings);

} // namespace audiopix
