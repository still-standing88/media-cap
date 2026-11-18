#include "common.hpp"
#include <atomic>
int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<monitor-index> <audio-input-index>")) return 0;
    if (argc != 3) throw std::invalid_argument("Usage: 12_callbacks <monitor-index> <audio-input-index>");
    using namespace std::chrono_literals;
    struct Counts { std::atomic<uint64_t> audio{0}, video{0}; } counts;
    const auto monitor = MediaCap::get_monitors().at(example::number(argv[1]));
    const auto device = MediaCap::get_audio_devices().at(example::number(argv[2]));
    if (device.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT) throw std::invalid_argument("Select an audio input device");

    MediaCap::Session session;
    session.add_source(monitor, MediaCap::ScreenCaptureConfig{33});
    session.add_source(device, {MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
    session.set_frame_callbacks(
        [](size_t index, MediaCap_SourceType, const MediaCap_AudioFrameView*, void* context) {
            if (index == 1) ++static_cast<Counts*>(context)->audio;
        },
        [](size_t index, MediaCap_SourceType, const MediaCap_VideoFrameView*, void* context) {
            if (index == 0) ++static_cast<Counts*>(context)->video;
        }, &counts);

    const bool started = session.start_capture();
    example::require(started, session.get_error());
    std::this_thread::sleep_for(2s);
    session.stop_capture();
    example::fail(session.get_error());
    std::cout << counts.audio << " audio blocks, " << counts.video << " video frames\n";
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
