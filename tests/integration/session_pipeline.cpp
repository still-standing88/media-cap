#include "../support/test.hpp"
#include "core/SessionImpl.h"
#include "../support/frames.hpp"
#include "../support/temporary_output.hpp"

extern "C" {
#include <libavformat/avformat.h>
}

using namespace MediaCap;
using namespace std::chrono_literals;

namespace {
class Source : public Capturer {
public:
    explicit Source(MediaCap_SourceType type, bool deliver = true) : deliver_(deliver) {
        m_source_type = type;
    }

    ~Source() override { stop(); }

    void start() override {
        m_is_running.store(true);
        if (!deliver_) return;
        worker_ = std::thread([this] {
            auto timestamp = RecordingTimeline::now_ns();
            if (m_source_type == MEDIACAP_SOURCE_TYPE_MONITOR) {
                m_frame_callback(fixture::video(timestamp), -1);
                return;
            }
            while (m_is_running.load()) {
                m_frame_callback(fixture::audio(timestamp), -1);
                timestamp += 10000000;
                std::this_thread::sleep_until(std::chrono::steady_clock::time_point(
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(timestamp))));
            }
        });
    }

    void stop() override {
        m_is_running.store(false);
        if (worker_.joinable()) worker_.join();
    }

private:
    bool deliver_;
    std::thread worker_;
};

struct SessionFixture {
    std::shared_ptr<SessionImpl> session = std::make_shared<SessionImpl>();
    ~SessionFixture() { session->shutdown(); }

    void add_sources(bool video_ready = true) {
        session->capturers.push_back(std::make_unique<Source>(MEDIACAP_SOURCE_TYPE_MONITOR, video_ready));
        session->capturers.push_back(std::make_unique<Source>(MEDIACAP_SOURCE_TYPE_AUDIO_INPUT));
    }
};

struct MuxConfig {
    MediaCap_VideoEncoderConfig video{64, 64, 30, 200000, MEDIACAP_VIDEO_CODEC_H264};
    MediaCap_AudioEncoderConfig audio{48000, 1, 0, MEDIACAP_AUDIO_CODEC_PCM};
    std::string name;
    MediaCap_RecordingConfig config;

    explicit MuxConfig(const fixture::TemporaryOutput& output)
        : name(output.path.string()), config{name.c_str(), MEDIACAP_CONTAINER_MKV, &video, &audio} {}
};

struct Captured {
    std::atomic<unsigned> video{0}, audio{0};

    void attach(SessionImpl& session) {
        REQUIRE(session.set_frame_callbacks(
            [](size_t index, MediaCap_SourceType type, const MediaCap_AudioFrameView* audio, void* data) {
                CHECK(index == 1);
                CHECK(type == MEDIACAP_SOURCE_TYPE_AUDIO_INPUT);
                CHECK(audio->num_samples == 480);
                CHECK(audio->format.sample_rate == 48000);
                ++static_cast<Captured*>(data)->audio;
            },
            [](size_t index, MediaCap_SourceType type, const MediaCap_VideoFrameView* video, void* data) {
                CHECK(index == 0);
                CHECK(type == MEDIACAP_SOURCE_TYPE_MONITOR);
                CHECK(video->data[0][4] == 1);
                ++static_cast<Captured*>(data)->video;
            }, this));
    }
};
}

TEST_CASE("Capture-only routes raw audio and the first still image") {
    Captured frames;
    SessionFixture fixture;
    fixture.add_sources();
    CHECK_FALSE(fixture.session->start_capture());
    frames.attach(*fixture.session);
    REQUIRE(fixture.session->start_capture());
    CHECK(fixture.session->is_capturing());
    CHECK_FALSE(fixture.session->recording.load());
    CHECK_FALSE(fixture.session->set_frame_callbacks(nullptr, nullptr, nullptr));
    fixture.session->shutdown();
    CHECK(frames.audio.load() > 0);
    CHECK(frames.video.load() == 1);
    CHECK(fixture.session->capturers.empty());
}

TEST_CASE("Timed recording holds a still image and finalizes matching A/V tracks") {
    fixture::TemporaryOutput output("-timed.mkv");
    MuxConfig config(output);
    Captured frames;
    SessionFixture fixture;
    fixture.add_sources();
    frames.attach(*fixture.session);
    REQUIRE(fixture.session->start_recording(config.config, 200ms, 2s));
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (fixture.session->recording && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);
    CHECK_FALSE(fixture.session->recording.load());
    fixture.session->shutdown();
    REQUIRE_FALSE(fixture.session->errors.failed());
    CHECK(frames.audio.load() > 0);
    CHECK(frames.video.load() == 1);

    struct Input {
        AVFormatContext* file = nullptr;
        ~Input() { avformat_close_input(&file); }
    } input;
    REQUIRE(avformat_open_input(&input.file, config.name.c_str(), nullptr, nullptr) == 0);
    REQUIRE(avformat_find_stream_info(input.file, nullptr) == 0);
    const int video = av_find_best_stream(input.file, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    const int audio = av_find_best_stream(input.file, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    REQUIRE(video >= 0);
    REQUIRE(audio >= 0);
    CHECK(input.file->streams[video]->codecpar->codec_id == AV_CODEC_ID_H264);
    CHECK(input.file->streams[audio]->codecpar->codec_id == AV_CODEC_ID_PCM_S16LE);

    AVPacket packet{};
    int video_packets = 0;
    int64_t audio_samples = 0;
    while (av_read_frame(input.file, &packet) >= 0) {
        if (packet.stream_index == video) {
            if (video_packets == 0) CHECK(packet.pts == 0);
            ++video_packets;
        }
        if (packet.stream_index == audio) audio_samples += packet.size / 2;
        av_packet_unref(&packet);
    }
    CHECK(video_packets == 6);
    CHECK(audio_samples == 9600);
}

TEST_CASE("A missing first frame times out before the output opens and allows reuse") {
    fixture::TemporaryOutput output("-unready.mkv");
    MuxConfig config(output);
    SessionFixture fixture;
    fixture.add_sources(false);
    CHECK_FALSE(fixture.session->start_recording(config.config, 100ms, 60ms));
    CHECK(fixture.session->errors.get().code == MEDIACAP_ERROR_TIMEOUT);
    CHECK(fixture.session->capturers.empty());
    CHECK_FALSE(std::filesystem::exists(output.path));

    fixture.add_sources();
    REQUIRE(fixture.session->start_recording(config.config, 100ms, 2s));
    fixture.session->shutdown();
    CHECK_FALSE(fixture.session->errors.failed());
}
