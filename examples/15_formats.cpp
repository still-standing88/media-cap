#include "common.hpp"
#include <algorithm>
#include <iterator>

int main(int argc, char** argv) try {
    constexpr auto args = "<audio|video> <source-index> <format> <output-file>";
    if (example::usage(argc, argv, args)) {
        std::cout << "Audio formats: wav, flac, opus, mp3, m4a\nVideo formats: mp4, webm, mkv\n";
        return 0;
    }
    if (argc != 5) throw std::invalid_argument(std::string("Usage: 15_formats ") + args);

    using namespace std::chrono_literals;
    const std::string_view kind(argv[1]);
    const auto index = example::number(argv[2]);
    const std::string_view format(argv[3]);
    MediaCap::Session session;
    MediaCap::RecordingConfig output{argv[4], MEDIACAP_CONTAINER_DEFAULT, nullptr, nullptr};
    MediaCap_VideoEncoderConfig video{960, 540, 30, 1'000'000, MEDIACAP_VIDEO_CODEC_H264};
    MediaCap_AudioEncoderConfig audio{48000, 1, 64000, MEDIACAP_AUDIO_CODEC_PCM};
    if (kind == "audio") {
        struct Format { std::string_view name; MediaCap_ContainerFormat container; MediaCap_AudioCodec codec; };
        const Format formats[] = {
            {"wav", MEDIACAP_CONTAINER_WAV, MEDIACAP_AUDIO_CODEC_PCM},
            {"flac", MEDIACAP_CONTAINER_FLAC, MEDIACAP_AUDIO_CODEC_FLAC},
            {"opus", MEDIACAP_CONTAINER_OPUS, MEDIACAP_AUDIO_CODEC_OPUS},
            {"mp3", MEDIACAP_CONTAINER_MP3, MEDIACAP_AUDIO_CODEC_MP3},
            {"m4a", MEDIACAP_CONTAINER_M4A, MEDIACAP_AUDIO_CODEC_AAC},
        };
        const auto* selected = std::find_if(std::begin(formats), std::end(formats),
            [&](const Format& option) { return option.name == format; });
        if (selected == std::end(formats)) throw std::invalid_argument("Choose wav, flac, opus, mp3, or m4a");
        const auto source = MediaCap::get_audio_devices().at(index);
        if (source.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT)
            throw std::invalid_argument("Select an audio input device");
        session.add_source(source, {MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
        audio.codec = selected->codec;
        output.container = selected->container;
        output.audio_config = &audio;
    } else if (kind == "video") {
        if (format == "mp4") {
            output.container = MEDIACAP_CONTAINER_MP4;
        } else if (format == "webm") {
            output.container = MEDIACAP_CONTAINER_WEBM;
            video.codec = MEDIACAP_VIDEO_CODEC_VP9;
        } else if (format == "mkv") {
            output.container = MEDIACAP_CONTAINER_MKV;
            video.codec = MEDIACAP_VIDEO_CODEC_AV1;
        } else {
            throw std::invalid_argument("Choose mp4, webm, or mkv");
        }
        session.add_source(MediaCap::get_monitors().at(index), MediaCap::ScreenCaptureConfig{33});
        output.video_config = &video;
    } else {
        throw std::invalid_argument("Choose audio or video");
    }

    example::record_for(session, output, 2s);
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
