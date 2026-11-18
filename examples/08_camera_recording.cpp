#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<camera-index> <output.mkv>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 08_camera_recording <camera-index> <output.mkv>");
    const auto source = MediaCap::get_camera_devices().at(example::number(argv[1]));
    MediaCap::Session session;
    session.add_source(source, MediaCap::CameraCaptureConfig{640, 480, 30, MEDIACAP_PIXEL_FORMAT_BGRA});

    MediaCap_VideoEncoderConfig video{640, 480, 30, 3'000'000, MEDIACAP_VIDEO_CODEC_H264};
    const MediaCap::RecordingConfig output{argv[2], MEDIACAP_CONTAINER_MKV, &video, nullptr};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
