#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor-index> <output.mp4> <seconds>")) return 0;
    if (argc != 4) throw std::invalid_argument("Usage: 10_timed_session <monitor-index> <output.mp4> <seconds>");
    const auto monitor = MediaCap::get_monitors().at(example::number(argv[1]));
    const auto duration = std::chrono::seconds(example::number(argv[3]));
    if (duration.count() == 0) throw std::invalid_argument("Duration must be positive");

    MediaCap::Session session;
    session.add_source(monitor, MediaCap::ScreenCaptureConfig{33});
    MediaCap_VideoEncoderConfig video{1280, 720, 30, 2'000'000, MEDIACAP_VIDEO_CODEC_H264};
    const MediaCap::RecordingConfig output{argv[2], MEDIACAP_CONTAINER_MP4, &video, nullptr};
    example::record_for(session, output, duration);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
