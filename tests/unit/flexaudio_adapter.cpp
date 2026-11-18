#include "../support/test.hpp"
#include "capture/FlexAudioCapturer.h"
#include "integrations/flexaudio_bridge/bridge.h"
#include "utils/RecordingTimeline.h"
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

using namespace MediaCap;
namespace {
struct Fake {
    unsigned index = 0;
    bool stopped = false;
    std::thread::id worker = std::this_thread::get_id();
};
std::atomic<int> freed{0}, closed{0};
bool fail = false;
std::string expected_id(512, 'x');
void worker(void *p) { CHECK(static_cast<Fake *>(p)->worker == std::this_thread::get_id()); }
} // namespace
extern "C" {
int32_t mc_flex_validate(const MediaCap_AudioSourceConfig *, McFlexError *) { return 0; }
int32_t mc_flex_open(const MediaCap_AudioSourceConfig *c, void **p, McFlexError *) {
    CHECK(c->device_id == expected_id);
    CHECK(c->exclude_pid_count == 2);
    if (c->exclude_pid_count == 2) CHECK(c->exclude_pids[1] == 23);
    *p = new Fake;
    return 0;
}
int32_t mc_flex_start(void *p, McFlexError *) {
    worker(p);
    return 0;
}
int32_t mc_flex_stop(void *p, McFlexError *) {
    worker(p);
    static_cast<Fake *>(p)->stopped = true;
    return 0;
}
void mc_flex_free(void *p) {
    worker(p);
    CHECK(static_cast<Fake *>(p)->stopped);
    delete static_cast<Fake *>(p);
    ++closed;
}
void mc_flex_chunk_free(void *p) {
    if (p) {
        delete[] static_cast<float *>(p);
        ++freed;
    }
}
int32_t mc_flex_poll(void *p, McFlexChunk *out, McFlexError *error) {
    worker(p);
    auto &f = *static_cast<Fake *>(p);
    if (f.index == 3 && fail) {
        *error = {};
        error->code = 3;
        error->native_code = 123;
        std::strcpy(error->message, "test device lost");
        return -1;
    }
    if (f.index >= 4 || (f.index == 3 && !f.stopped))
        return 0;
    auto *data = new float[1920];
    std::fill(data, data + 1920, float(f.index + 1));
    const int64_t arrivals[] = {0, 17000000, 55000000, 60000000};
    *out = {data, 1920, 960, 48000, 2, int64_t(f.index) * 20000000, arrivals[f.index], data};
    ++f.index;
    return 1;
}
}
TEST_CASE("The FlexAudio adapter copies configuration, keeps PCM alive and reports worker failures") {
    for (bool with_failure : {false, true}) {
        fail = with_failure;
        freed = 0;
        closed = 0;
        MediaCap_AudioSourceConfig config{};
        config.kind = MEDIACAP_AUDIO_CAPTURE_SYSTEM;
        std::unique_ptr<FlexAudioCapturer> capturer;
        { // All caller-owned storage expires before the worker reads it.
            std::string id = expected_id;
            uint32_t pids[] = {17, 23};
            config.device_id = id.c_str();
            config.exclude_pids = pids;
            config.exclude_pid_count = 2;
            capturer = std::make_unique<FlexAudioCapturer>(config);
        }
        Errors::State state;
        capturer->set_error_context(&state, 4);
        std::mutex mutex;
        std::condition_variable ready;
        std::vector<std::shared_ptr<PackedFrame>> retained;
        std::vector<int64_t> observed;
        capturer->set_frame_callback([&](std::shared_ptr<PackedFrame> frame, int64_t time) {
            std::lock_guard<std::mutex> lock(mutex);
            retained.push_back(std::move(frame));
            observed.push_back(time);
            ready.notify_one();
        });
        const auto before = RecordingTimeline::now_ns();
        capturer->start();
        {
            std::unique_lock<std::mutex> lock(mutex);
            CHECK(ready.wait_for(lock, std::chrono::seconds(5), [&] { return retained.size() >= 3; }));
        }
        capturer->stop();
        capturer.reset();
        CHECK(closed.load() == 1);
        CHECK(freed.load() == 0);
        REQUIRE(retained.size() == (with_failure ? 3u : 4u));
        const auto first = std::get<MediaCap_AudioFrame>(*retained[0]).timestamp_ns;
        CHECK(first >= before - 20000000);
        CHECK(first <= RecordingTimeline::now_ns());
        for (size_t i = 0; i < retained.size(); ++i) {
            const auto &f = std::get<MediaCap_AudioFrame>(*retained[i]);
            CHECK(f.timestamp_ns == first + int64_t(i) * 20000000);
            CHECK(reinterpret_cast<float *>(f.data[0])[1919] == float(i + 1));
        }
        CHECK(observed[2] - observed[0] == 55000000); // Arrival jitter remains an observation only.
        CHECK(state.failed() == with_failure);
        if (with_failure) {
            const auto error = state.get();
            CHECK(error.code == MEDIACAP_ERROR_DEVICE_LOST);
            CHECK(error.native_code == 123);
            CHECK(error.source_index == 4);
        }
        const auto count = retained.size();
        retained.clear();
        CHECK(freed.load() == int(count));
    }
}
