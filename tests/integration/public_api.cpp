#include "../support/test.hpp"
#include "../../src/utils/Utf8.h"
#include <MediaCap.hpp>
#include <stdexcept>
#include <utility>

extern "C" int exercise_c_api(void);

TEST_CASE("A C consumer links the session and screenshot exports") {
    CHECK(exercise_c_api() == 1);
}

TEST_CASE("Display text repair preserves UTF-8 character boundaries") {
    char cropped[5]{};
    MediaCap::Utf8::copy(cropped, "ab\xE7\x94\xBB");
    CHECK(MediaCap::Utf8::bounded(cropped) == "ab");

    char repaired[8]{};
    MediaCap::Utf8::copy(repaired, "a\xFF" "b");
    CHECK(MediaCap::Utf8::bounded(repaired) == "a\xEF\xBF\xBD" "b");
    CHECK(MediaCap::Utf8::valid(MediaCap::Utf8::bounded(repaired)));
    CHECK_FALSE(MediaCap::Utf8::valid("\xED\xA0\x80"));
    CHECK_FALSE(MediaCap::Utf8::valid("\xF4\x90\x80\x80"));
}

TEST_CASE("A moved C++ session retains identity and structured diagnostics") {
    struct ResetCallback { ~ResetCallback() { MediaCap::set_error_callback({}); } } reset;
    int calls = 0;
    MediaCap::Error delivered{};
    MediaCap::set_error_callback([&](const auto& error) {
        ++calls;
        delivered = error;
        throw std::runtime_error("Exceptions stay in the callback");
    });

    MediaCap::Session session;
    const auto id = session.id();
    MediaCap::Session moved(std::move(session));
    CHECK(session.id() == 0);
    CHECK(moved.id() == id);
    CHECK_FALSE(moved.start_recording({}));
    CHECK(calls == 1);
    CHECK(moved.has_error());
    CHECK(moved.get_error().code == MEDIACAP_ERROR_INVALID_ARGUMENT);
    CHECK(delivered.session_id == id);

    MediaCap::clear_last_error();
    CHECK(MediaCap::get_last_error().code == MEDIACAP_ERROR_NONE);
    CHECK(moved.has_error());
}

TEST_CASE("Audio capabilities agree with the library build option") {
    MediaCap_AudioCapabilities capabilities{};
    REQUIRE(MediaCap_GetAudioCapabilities(&capabilities));
    CHECK(capabilities.backend_enabled == MEDIACAP_EXPECT_FLEXAUDIO);
    const auto config = MediaCap_DefaultAudioSourceConfig();
    CHECK(config.kind == MEDIACAP_AUDIO_CAPTURE_MIC);
    CHECK(config.gain == 1.0f);
}
