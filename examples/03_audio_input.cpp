#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<audio-device-index> <output.wav>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 03_audio_input <audio-device-index> <output.wav>");
    const auto device = MediaCap::get_audio_devices().at(example::number(argv[1]));
    if (device.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT) throw std::invalid_argument("Select an audio input device");

    MediaCap::Session session;
    session.add_source(device, {MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
    MediaCap_AudioEncoderConfig audio{48000, 1, 0, MEDIACAP_AUDIO_CODEC_PCM};
    const MediaCap::RecordingConfig output{argv[2], MEDIACAP_CONTAINER_WAV, nullptr, &audio};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
