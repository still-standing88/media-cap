#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "")) return 0;
    if (argc != 1) throw std::invalid_argument("Usage: 02_source_info ");
    for (const auto& monitor : MediaCap::get_monitors())
        std::cout << monitor.name << ' ' << monitor.width << 'x' << monitor.height
                  << " at (" << monitor.offsetX << ", " << monitor.offsetY << "), scale " << monitor.scaling << '\n';

    for (const auto& window : MediaCap::get_windows())
        std::cout << window.name << " handle " << window.handle << ' ' << window.width << 'x' << window.height << '\n';

    for (const auto& camera : MediaCap::get_cameras()) {
        std::cout << camera.camera.name << '\n';
        for (const auto& resolution : camera.supported_resolutions)
            std::cout << "  " << resolution.width << 'x' << resolution.height << '\n';
        for (const auto format : camera.supported_pixel_formats)
            std::cout << "  pixel format " << format << '\n';
    }

    for (const auto& device : MediaCap::get_audio_devices())
        std::cout << device.name << ", device type " << device.type
                  << (device.is_default ? ", default" : "") << '\n';

    const auto capabilities = MediaCap::get_audio_capabilities();
    if (capabilities.backend_enabled) {
        for (const auto& device : MediaCap::get_audio_capture_devices())
            std::cout << device.name << ", " << device.sample_rate << " Hz, " << device.channels
                      << " channels" << (device.is_default ? ", default" : "") << '\n';
        std::cout << "Process support " << capabilities.process << ", PID scope " << capabilities.process_scope << '\n';
    }
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
