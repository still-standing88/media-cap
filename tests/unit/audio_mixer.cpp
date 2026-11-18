#include "../support/test.hpp"
#include "audio/AudioMixer.h"
#include "utils/RecordingTimeline.h"
#include "../support/frames.hpp"
#include <algorithm>

using namespace MediaCap;

TEST_CASE("Resampling and mixing preserve length and headroom") {
    std::vector<float> samples;
    std::vector<int64_t> timestamps;
    AudioMixer mixer({MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
    const auto left = mixer.add_input();
    const auto right = mixer.add_input();
    mixer.set_output_callback([&](const auto& frame) {
        const auto& audio = std::get<MediaCap_AudioFrame>(*frame);
        const auto* data = reinterpret_cast<const float*>(audio.data[0]);
        timestamps.push_back(audio.timestamp_ns);
        samples.insert(samples.end(), data, data + audio.num_samples);
    });
    mixer.start();
    for (int i = 0; i < 10; ++i) {
        REQUIRE(mixer.add_frame(left, fixture::audio(i * 10000000LL, 441, 44100)));
        REQUIRE(mixer.add_frame(right, fixture::audio(i * 10000000LL, 480, 48000)));
    }
    mixer.stop();
    CHECK(mixer.healthy());
    CHECK(mixer.dropped_frames() == 0);
    REQUIRE(samples.size() == 4800);
    CHECK(samples[2400] == doctest::Approx(.25f));
    REQUIRE_FALSE(timestamps.empty());
    CHECK(timestamps.front() == 0);
    CHECK(std::is_sorted(timestamps.begin(), timestamps.end()));
}
