#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "")) return 0;
    if (argc != 1) throw std::invalid_argument("Usage: 01_source_discovery ");
    const auto monitors = MediaCap::get_monitors();
    for (size_t i = 0; i < monitors.size(); ++i) std::cout << "monitor " << i << "  " << monitors[i].name << '\n';

    const auto windows = MediaCap::get_windows();
    for (size_t i = 0; i < windows.size(); ++i) std::cout << "window " << i << "  " << windows[i].name << '\n';

    const auto cameras = MediaCap::get_camera_devices();
    for (size_t i = 0; i < cameras.size(); ++i) std::cout << "camera " << i << "  " << cameras[i].name << '\n';

    const auto audio = MediaCap::get_audio_devices();
    for (size_t i = 0; i < audio.size(); ++i) std::cout << "audio device " << i << "  " << audio[i].name << '\n';

    if (MediaCap::get_audio_capabilities().backend_enabled) {
        for (const auto& device : MediaCap::get_audio_capture_devices())
            std::cout << "FlexAudio device  " << device.id << "  " << device.name << '\n';
        for (const auto& process : MediaCap::get_audio_processes())
            std::cout << "audio process " << process.pid << "  " << process.name << '\n';
    }
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
