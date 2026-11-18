#pragma once
#include "MediaCap.hpp"

namespace MediaCap::Native {
std::vector<Monitor> get_monitors();
std::vector<Window> get_windows();
std::vector<CameraDetail> get_cameras();
std::vector<Camera> get_camera_devices();
std::vector<AudioDevice> get_audio_devices();
}
