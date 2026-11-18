#include "common.hpp"
#include "image.hpp"
#include <mutex>
int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor-index> <output.ppm>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 13_video_frames <monitor-index> <output.ppm>");
    struct Result {
        std::mutex mutex;
        example::Image image;
    } result;

    MediaCap::Session session;
    session.add_source(MediaCap::get_monitors().at(example::number(argv[1])), MediaCap::ScreenCaptureConfig{33});
    session.set_frame_callbacks(nullptr,
        [](size_t, MediaCap_SourceType, const MediaCap_VideoFrameView* frame, void* context) {
            auto& result = *static_cast<Result*>(context);
            std::lock_guard<std::mutex> lock(result.mutex);
            if (result.image.rgb.empty()) result.image.copy_bgra(*frame);
        }, &result);

    const bool started = session.start_capture();
    example::require(started, session.get_error());
    session.stop_capture();
    example::fail(session.get_error());
    if (result.image.rgb.empty()) throw std::runtime_error("No image was delivered");
    result.image.save_ppm(argv[2]);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
