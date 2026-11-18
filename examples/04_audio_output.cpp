#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<output.wav> [device-id]")) return 0;
    if (argc < 2 || argc > 3) throw std::invalid_argument("Usage: 04_audio_output <output.wav> [device-id]");
    auto source = MediaCap::default_audio_source_config();
    source.kind = MEDIACAP_AUDIO_CAPTURE_SYSTEM;
    if (argc == 3) source.device_id = argv[2];

    MediaCap::Session session;
    const bool added = session.add_audio_source(source);
    example::require(added, session.get_error());
    MediaCap_AudioEncoderConfig audio{48000, 2, 0, MEDIACAP_AUDIO_CODEC_PCM};
    const MediaCap::RecordingConfig output{argv[1], MEDIACAP_CONTAINER_WAV, nullptr, &audio};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
