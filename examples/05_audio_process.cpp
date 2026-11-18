#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<pid> <output.wav>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 05_audio_process <pid> <output.wav>");
    const auto capabilities = MediaCap::get_audio_capabilities();
    if (capabilities.process != MEDIACAP_AUDIO_AVAILABLE)
        throw std::runtime_error(capabilities.process_detail[0] ? capabilities.process_detail : "Process audio is unavailable");

    auto source = MediaCap::default_audio_source_config();
    source.kind = MEDIACAP_AUDIO_CAPTURE_PROCESS;
    source.process_mode = MEDIACAP_PROCESS_INCLUDE;
    source.process_id = example::number(argv[1]);

    MediaCap::Session session;
    const bool added = session.add_audio_source(source);
    example::require(added, session.get_error());
    MediaCap_AudioEncoderConfig audio{48000, 2, 0, MEDIACAP_AUDIO_CODEC_PCM};
    const MediaCap::RecordingConfig output{argv[2], MEDIACAP_CONTAINER_WAV, nullptr, &audio};
    example::record_until_enter(session, output);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
