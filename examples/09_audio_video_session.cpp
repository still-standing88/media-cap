#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor-index> <audio-input-index> <output.mkv>")) return 0;
    if (argc != 4) throw std::invalid_argument("Usage: 09_audio_video_session <monitor-index> <audio-input-index> <output.mkv>");
    const auto monitor = MediaCap::get_monitors().at(example::number(argv[1]));
    const auto device = MediaCap::get_audio_devices().at(example::number(argv[2]));
    if (device.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT) throw std::invalid_argument("Select an audio input device");

    MediaCap::Session session;
    session.add_source(monitor, MediaCap::ScreenCaptureConfig{33});
    session.add_source(device, {MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
    MediaCap_VideoEncoderConfig video{monitor.width, monitor.height, 30, 3'000'000, MEDIACAP_VIDEO_CODEC_H264};
    MediaCap_AudioEncoderConfig audio{48000, 1, 0, MEDIACAP_AUDIO_CODEC_PCM};
    const MediaCap::RecordingConfig output{argv[3], MEDIACAP_CONTAINER_MKV, &video, &audio};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
