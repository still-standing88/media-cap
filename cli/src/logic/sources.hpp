#pragma once

#include <MediaCap.hpp>

#include <optional>
#include <string>
#include <vector>

namespace mccli {
struct AudioSource;
MediaCap::Monitor select_monitor(std::optional<int> id);
MediaCap::Window select_window(uint64_t handle, const std::string& title);
MediaCap::Camera select_camera(int index);
void add_audio(MediaCap::Session& session, const std::vector<AudioSource>& sources);
}
