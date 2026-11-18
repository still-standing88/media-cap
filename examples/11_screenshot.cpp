#include "common.hpp"
#include "image.hpp"
int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor|window> <index> <output.ppm> [x y width height output-width output-height]")) return 0;
    if (argc != 4 && argc != 10) throw std::invalid_argument("Usage: 11_screenshot <monitor|window> <index> <output.ppm> [x y width height output-width output-height]");
    const auto index = example::number(argv[2]);
    MediaCap_ScreenshotConfig config{};
    config.timeout_ms = 3000;
    if (argc == 10) {
        config.x = example::number(argv[4]);
        config.y = example::number(argv[5]);
        config.width = example::number(argv[6]);
        config.height = example::number(argv[7]);
        config.output_width = example::number(argv[8]);
        config.output_height = example::number(argv[9]);
    }

    example::Image image;
    auto receive = [](const MediaCap_VideoFrameView* frame, void* context) {
        static_cast<example::Image*>(context)->copy_bgra(*frame);
    };

    bool captured = false;
    const std::string_view kind(argv[1]);
    if (kind == "monitor")
        captured = MediaCap::capture_screenshot(MediaCap::get_monitors().at(index), receive, &image, config);
    else if (kind == "window")
        captured = MediaCap::capture_screenshot(MediaCap::get_windows().at(index), receive, &image, config);
    else
        throw std::invalid_argument("Source kind must be monitor or window");

    example::require(captured, MediaCap::get_last_error());
    image.save_ppm(argv[3]);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
