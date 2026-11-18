#include "../support/test.hpp"
#include "MediaCap_c.h"
#include "capture/Screenshot.h"
#include "../support/frames.hpp"

namespace {
class StillSource : public MediaCap::Capturer {
public:
    explicit StillSource(bool ready = true) : ready_(ready) {}
    void start() override {
        if (ready_) m_frame_callback(fixture::video(123), -1);
    }
    void stop() override {}
private:
    bool ready_;
};
}

TEST_CASE("Screenshot crops in source coordinates and returns resized BGRA") {
    int calls = 0;
    const MediaCap_ScreenshotConfig config{2, 3, 6, 4, 3, 2, 100};
    REQUIRE(MediaCap::capture_screenshot(std::make_unique<StillSource>(), config,
        [](const MediaCap_VideoFrameView* image, void* state) {
            ++*static_cast<int*>(state);
            CHECK(image->width == 3);
            CHECK(image->height == 2);
            CHECK(image->linesize[0] == 12);
            CHECK(image->pixel_format == MEDIACAP_PIXEL_FORMAT_BGRA);
            CHECK(image->timestamp_ns == 123);
            CHECK(image->data[0][0] == 32);
            CHECK(image->data[0][4] == 34);
            CHECK(image->data[0][8] == 36);
            CHECK(image->data[0][12] == 52);
        }, &calls));
    CHECK(calls == 1);
}

TEST_CASE("Out-of-bounds and timed-out screenshots deliver no pixels") {
    int calls = 0;
    auto callback = [](const MediaCap_VideoFrameView*, void* state) { ++*static_cast<int*>(state); };
    MediaCap_ScreenshotConfig config{60, 3, 6, 4, 3, 2, 20};
    CHECK_FALSE(MediaCap::capture_screenshot(std::make_unique<StillSource>(), config, callback, &calls));
    MediaCap_Error error{};
    REQUIRE(MediaCap_GetLastError(&error));
    CHECK(error.code == MEDIACAP_ERROR_INVALID_ARGUMENT);

    config = {};
    config.timeout_ms = 20;
    CHECK_FALSE(MediaCap::capture_screenshot(std::make_unique<StillSource>(false), config, callback, &calls));
    REQUIRE(MediaCap_GetLastError(&error));
    CHECK(error.code == MEDIACAP_ERROR_TIMEOUT);
    CHECK(calls == 0);
}
