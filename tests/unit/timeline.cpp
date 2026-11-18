#include "../support/test.hpp"
#include "utils/RecordingTimeline.h"
#include "utils/ThreadSafeQueue.h"

using namespace MediaCap;
using namespace std::chrono_literals;

TEST_CASE("Pause splits an audio block and collapses the video timeline") {
    RecordingTimeline timeline;
    timeline.start(0);
    timeline.set_paused(true, 250000000);
    timeline.set_paused(false, 750000000);
    CHECK(timeline.stop(1000000000) == 500000000);

    const auto spans = timeline.map_audio(0, 1000, 1000);
    REQUIRE(spans.size() == 2);
    CHECK(spans[0].offset == 0);
    CHECK(spans[0].count == 250);
    CHECK(spans[1].offset == 750);
    CHECK(spans[1].count == 250);
    CHECK(spans[1].timestamp_ns == 250000000);
    CHECK_FALSE(timeline.map_video(500000000).has_value());
    CHECK(timeline.map_video(800000000).value() == 300000000);
}

TEST_CASE("The timeline clips samples at start and deadline") {
    RecordingTimeline timeline;
    timeline.start(10000000);
    timeline.set_deadline(30000000);
    const auto spans = timeline.map_audio(0, 100, 1000);
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].offset == 10);
    CHECK(spans[0].count == 20);
}

TEST_CASE("A bounded queue preserves FIFO order and times out when empty") {
    Utils::ThreadSafeQueue<int> queue;
    CHECK(queue.try_push(1, 2));
    CHECK(queue.try_push(2, 2));
    CHECK_FALSE(queue.try_push(3, 2));
    CHECK(queue.wait_and_pop() == 1);
    int value = 0;
    CHECK(queue.wait_and_pop_for(value, 5ms));
    CHECK(value == 2);
    CHECK_FALSE(queue.wait_and_pop_for(value, 5ms));
}
