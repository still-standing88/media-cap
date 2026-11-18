#include "../support/test.hpp"
#include "../support/frames.hpp"
#include "../support/temporary_output.hpp"
#include "integrations/FFmpegMuxer.h"
#include "integrations/FFmpegPolicy.h"
#include "utils/RecordingTimeline.h"
#include <cerrno>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace {
struct Decoder {
    AVFormatContext* input = nullptr;
    AVCodecContext* codec = nullptr;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int stream = -1;

    ~Decoder() {
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
        avformat_close_input(&input);
    }

    void open(const std::string& path, AVMediaType type, AVCodecID expected) {
        REQUIRE(packet);
        REQUIRE(frame);
        REQUIRE(avformat_open_input(&input, path.c_str(), nullptr, nullptr) == 0);
        REQUIRE(avformat_find_stream_info(input, nullptr) == 0);
        const AVCodec* implementation = nullptr;
        stream = av_find_best_stream(input, type, -1, -1, &implementation, 0);
        REQUIRE(stream >= 0);
        CHECK(input->streams[stream]->codecpar->codec_id == expected);
        codec = avcodec_alloc_context3(implementation);
        REQUIRE(codec);
        REQUIRE(avcodec_parameters_to_context(codec, input->streams[stream]->codecpar) == 0);
        REQUIRE(avcodec_open2(codec, implementation, nullptr) == 0);
    }

    template<class Callback>
    void decode(Callback receive) {
        auto drain = [&] {
            int status;
            while ((status = avcodec_receive_frame(codec, frame)) == 0) {
                receive(*frame);
                av_frame_unref(frame);
            }
            CHECK((status == AVERROR(EAGAIN) || status == AVERROR_EOF));
        };
        while (av_read_frame(input, packet) >= 0) {
            if (packet->stream_index == stream) {
                REQUIRE(avcodec_send_packet(codec, packet) == 0);
                drain();
            }
            av_packet_unref(packet);
        }
        REQUIRE(avcodec_send_packet(codec, nullptr) == 0);
        drain();
    }
};
}

TEST_CASE("Each supported audio codec round-trips samples") {
    struct Format { MediaCap_AudioCodec choice; AVCodecID codec; const char* extension; };
    constexpr Format formats[] = {
        {MEDIACAP_AUDIO_CODEC_AAC, AV_CODEC_ID_AAC, ".m4a"},
        {MEDIACAP_AUDIO_CODEC_OPUS, AV_CODEC_ID_OPUS, ".opus"},
        {MEDIACAP_AUDIO_CODEC_VORBIS, AV_CODEC_ID_VORBIS, ".ogg"},
        {MEDIACAP_AUDIO_CODEC_FLAC, AV_CODEC_ID_FLAC, ".flac"},
        {MEDIACAP_AUDIO_CODEC_MP3, AV_CODEC_ID_MP3, ".mp3"},
        {MEDIACAP_AUDIO_CODEC_PCM, AV_CODEC_ID_PCM_S16LE, ".wav"},
    };
    for (const auto& format : formats) {
        CAPTURE(format.extension);
        fixture::TemporaryOutput output(format.extension);
        const auto path = output.path.string();
        MediaCap_AudioEncoderConfig audio{48000, 1, 64000, format.choice};
        const MediaCap_RecordingConfig config{path.c_str(), MEDIACAP_CONTAINER_DEFAULT, nullptr, &audio};
        MediaCap::FFmpegMuxer mux(config);
        REQUIRE(mux.start());
        for (int block = 0; block < 25; ++block)
            REQUIRE(mux.add_frame(fixture::audio(MediaCap::samples_to_ns(block * 480, 48000))));
        mux.stop();
        REQUIRE(mux.healthy());

        Decoder input;
        input.open(path, AVMEDIA_TYPE_AUDIO, format.codec);
        int samples = 0;
        input.decode([&](const AVFrame& frame) {
            CHECK(frame.sample_rate == 48000);
            CHECK(frame.ch_layout.nb_channels == 1);
            samples += frame.nb_samples;
        });
        CHECK(samples >= 11000);
        CHECK(samples <= 13500);
    }
}

TEST_CASE("H.264, VP9 and AV1 round-trip images in supported containers") {
    struct Format { MediaCap_VideoCodec choice; AVCodecID codec; const char* extension; };
    constexpr Format formats[] = {
        {MEDIACAP_VIDEO_CODEC_H264, AV_CODEC_ID_H264, ".mp4"},
        {MEDIACAP_VIDEO_CODEC_VP9, AV_CODEC_ID_VP9, ".webm"},
        {MEDIACAP_VIDEO_CODEC_AV1, AV_CODEC_ID_AV1, ".mkv"},
    };
    for (const auto& format : formats) {
        CAPTURE(format.extension);
        fixture::TemporaryOutput output(format.extension);
        const auto path = output.path.string();
        MediaCap_VideoEncoderConfig video{64, 64, 30, 200000, format.choice};
        const MediaCap_RecordingConfig config{path.c_str(), MEDIACAP_CONTAINER_DEFAULT, &video, nullptr};
        MediaCap::FFmpegMuxer mux(config);
        REQUIRE(mux.start());
        for (int frame = 0; frame < 6; ++frame)
            REQUIRE(mux.add_frame(fixture::video(frame * 1000000000LL / 30)));
        mux.stop(200000000);
        REQUIRE(mux.healthy());

        Decoder input;
        input.open(path, AVMEDIA_TYPE_VIDEO, format.codec);
        int frames = 0;
        input.decode([&](const AVFrame& frame) {
            CHECK(frame.width == 64);
            CHECK(frame.height == 64);
            ++frames;
        });
        CHECK(frames == 6);
    }
}

TEST_CASE("The muxer rejects unsupported video and container combinations") {
    fixture::TemporaryOutput output(".webm");
    const auto path = output.path.string();
    MediaCap_VideoEncoderConfig video{64, 64, 30, 200000, MEDIACAP_VIDEO_CODEC_H264};
    const MediaCap_RecordingConfig config{path.c_str(), MEDIACAP_CONTAINER_WEBM, &video, nullptr};
    MediaCap::FFmpegMuxer mux(config);
    CHECK_FALSE(mux.start());
    CHECK_FALSE(MediaCap::FFmpegPolicy::allowed("--enable-gpl", "LGPL version 2.1 or later"));
    CHECK_FALSE(MediaCap::FFmpegPolicy::allowed("--enable-nonfree", "LGPL"));
}
