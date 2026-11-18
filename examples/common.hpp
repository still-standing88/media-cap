#pragma once

#include <MediaCap.hpp>
#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace example {
using namespace std::chrono_literals;

inline bool usage(int argc, char** argv, std::string_view args) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << ' ' << args << '\n';
        return true;
    }
    return false;
}

inline unsigned number(const char* value) {
    const std::string_view text(value);
    unsigned result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Expected a nonnegative integer");
    return result;
}

inline void fail(const MediaCap::Error& error) {
    if (error.code != MEDIACAP_ERROR_NONE) throw std::runtime_error(error.message);
}

inline void require(bool ok, const MediaCap::Error& error) {
    if (ok) return;
    fail(error);
    throw std::runtime_error("MediaCap operation failed");
}

inline void record_until_enter(MediaCap::Session& session, const MediaCap::RecordingConfig& config) {
    const bool started = session.start_recording(config);
    require(started, session.get_error());
    std::cout << "Recording to " << config.output_filename << ". Press Enter to stop.\n";
    std::cin.get();
    session.stop_recording();
    fail(session.get_error());
}

inline void record_for(MediaCap::Session& session, const MediaCap::RecordingConfig& config,
                       std::chrono::milliseconds duration) {
    const bool started = session.start_recording(config, duration);
    require(started, session.get_error());
    while (session.is_recording()) std::this_thread::sleep_for(20ms);
    fail(session.get_error());
    std::cout << "Saved " << config.output_filename << '\n';
}

inline int report(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
}
