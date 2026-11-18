#include "common.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
int main(int argc, char** argv) try {
    if (example::usage(argc, argv, "<audio-input-index>")) return 0;
    if (argc != 2) throw std::invalid_argument("Usage: 14_audio_samples <audio-input-index>");
    using namespace std::chrono_literals;
    struct Levels {
        std::mutex mutex;
        uint64_t frames = 0;
        float peak = 0;
    } levels;
    const auto device = MediaCap::get_audio_devices().at(example::number(argv[1]));
    if (device.type != MEDIACAP_AUDIO_DEVICE_TYPE_INPUT) throw std::invalid_argument("Select an audio input device");

    MediaCap::Session session;
    session.add_source(device, {MEDIACAP_SAMPLE_FORMAT_FLT, 48000, 1});
    session.set_frame_callbacks(
        [](size_t, MediaCap_SourceType, const MediaCap_AudioFrameView* block, void* context) {
            if (block->format.format != MEDIACAP_SAMPLE_FORMAT_FLT) return;
            float peak = 0;
            const auto samples = size_t(block->num_samples) * block->format.channels;
            for (size_t i = 0; i < samples; ++i) {
                float sample;
                std::memcpy(&sample, block->data + i * sizeof(sample), sizeof(sample));
                peak = std::max(peak, std::abs(sample));
            }
            auto& levels = *static_cast<Levels*>(context);
            std::lock_guard<std::mutex> lock(levels.mutex);
            levels.peak = std::max(levels.peak, peak);
            levels.frames += block->num_samples;
        }, nullptr, &levels);

    const bool started = session.start_capture();
    example::require(started, session.get_error());
    std::this_thread::sleep_for(2s);
    session.stop_capture();
    example::fail(session.get_error());
    std::cout << levels.frames << " sample frames, linear peak " << levels.peak << '\n';
    return 0;
} catch (const std::exception& error) {
    return example::report(error);
}
