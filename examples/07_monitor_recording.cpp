#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor-index> <output.mkv>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 07_monitor_recording <monitor-index> <output.mkv>");
    const auto source = MediaCap::get_monitors().at(example::number(argv[1]));
    MediaCap::Session session;
    session.add_source(source, MediaCap::ScreenCaptureConfig{33});

    MediaCap_VideoEncoderConfig video{source.width, source.height, 30, 3'000'000, MEDIACAP_VIDEO_CODEC_H264};
    const MediaCap::RecordingConfig output{argv[2], MEDIACAP_CONTAINER_MKV, &video, nullptr};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
