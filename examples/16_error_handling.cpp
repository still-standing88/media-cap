#include "common.hpp"

int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "")) return 0;
    if (argc != 1) throw std::invalid_argument("Usage: 16_error_handling ");
    MediaCap::set_error_callback([](const MediaCap::Error& error) {
        std::cerr << "Error " << error.code << ", component " << error.component
                  << ", session " << error.session_id << ": " << error.message << '\n';
    });

    MediaCap::Session session;
    const MediaCap::RecordingConfig config{};
    const bool started = session.start_recording(config);
    const auto error = session.get_error();
    MediaCap::set_error_callback({});

    if (started || error.code != MEDIACAP_ERROR_INVALID_ARGUMENT)
        throw std::runtime_error("Expected an invalid-argument error for an empty session");
    std::cout << "Retained session error: " << error.message << '\n';
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
