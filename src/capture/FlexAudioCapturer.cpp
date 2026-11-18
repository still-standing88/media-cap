#include "FlexAudioCapturer.h"
#include "utils/RecordingTimeline.h"
#include "integrations/flexaudio_bridge/bridge.h"
#include <limits>
#include <stdexcept>

namespace MediaCap {
void report_flex_error(const Errors::Reporter &reporter, const McFlexError &error) {
    MediaCap_ErrorCode code;
    switch (error.code) {
    case 1:
        code = MEDIACAP_ERROR_INVALID_ARGUMENT;
        break;
    case 2:
        code = MEDIACAP_ERROR_DEVICE_UNAVAILABLE;
        break;
    case 3:
        code = MEDIACAP_ERROR_DEVICE_LOST;
        break;
    case 4:
        code = MEDIACAP_ERROR_UNSUPPORTED;
        break;
    case 5:
        code = MEDIACAP_ERROR_PERMISSION_DENIED;
        break;
    case 6:
        code = MEDIACAP_ERROR_BUFFER_OVERFLOW;
        break;
    case 8:
        code = MEDIACAP_ERROR_INTERNAL;
        break;
    default:
        code = MEDIACAP_ERROR_CAPTURE_FAILED;
        break;
    }
    reporter.report(code, error.message, error.native_code);
}
OwnedAudioSourceConfig::OwnedAudioSourceConfig(const MediaCap_AudioSourceConfig &config) : value(config) {
    if (config.device_id) {
        device = config.device_id;
        value.device_id = device->c_str();
    }
    if (config.mix_mic_device_id) {
        mic = config.mix_mic_device_id;
        value.mix_mic_device_id = mic->c_str();
    }
    if (config.mix_system_device_id) {
        system = config.mix_system_device_id;
        value.mix_system_device_id = system->c_str();
    }
    if (config.exclude_pid_count) {
        if (!config.exclude_pids)
            throw std::invalid_argument("Missing excluded PID array");
        excluded.assign(config.exclude_pids, config.exclude_pids + config.exclude_pid_count);
        value.exclude_pids = excluded.data();
    }
}
FlexAudioCapturer::FlexAudioCapturer(const MediaCap_AudioSourceConfig &config) : m_config(config) {
    switch (config.kind) {
    case MEDIACAP_AUDIO_CAPTURE_MIC:
        m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_INPUT;
        break;
    case MEDIACAP_AUDIO_CAPTURE_SYSTEM:
        m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT;
        break;
    case MEDIACAP_AUDIO_CAPTURE_PROCESS:
        m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_PROCESS;
        break;
    case MEDIACAP_AUDIO_CAPTURE_MIX:
        m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_MIX;
        break;
    }
}
bool FlexAudioCapturer::validate(const MediaCap_AudioSourceConfig &config, const Errors::Reporter &errors) {
#if MEDIACAP_HAS_FLEXAUDIO
    McFlexError error{};
    if (mc_flex_validate(&config, &error) < 0) {
        report_flex_error(errors, error);
        return false;
    }
    return true;
#else
    errors.report(MEDIACAP_ERROR_UNSUPPORTED, "flexaudio capture was disabled when MediaCap was built");
    return false;
#endif
}
void FlexAudioCapturer::start() {
    if (m_is_running.exchange(true))
        return;
    m_thread = std::thread([this] {
        try {
            run();
        } catch (...) {
            m_errors.exception("flexaudio capture worker failed");
        }
        m_is_running.store(false);
    });
}
void FlexAudioCapturer::stop() {
    m_is_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
}
void FlexAudioCapturer::run() {
#if MEDIACAP_HAS_FLEXAUDIO
    McFlexError error{};
    void *raw = nullptr;
    if (mc_flex_open(&m_config.value, &raw, &error) < 0) {
        report_flex_error(m_errors, error);
        return;
    }
    std::unique_ptr<void, decltype(&mc_flex_free)> stream(raw, mc_flex_free);
    if (mc_flex_start(raw, &error) < 0) {
        report_flex_error(m_errors, error);
        return;
    }
    std::optional<int64_t> anchor;
    int64_t arrival_offset = 0;
    struct Owner {
        McFlexChunk chunk{};
        uint8_t *plane = nullptr;
        PackedFrame frame;
        ~Owner() { mc_flex_chunk_free(chunk.owner); }
    };
    const auto poll = [&]() {
        McFlexChunk chunk{};
        const int result = mc_flex_poll(raw, &chunk, &error);
        if (result < 0) {
            report_flex_error(m_errors, error);
            return -1;
        }
        if (!result)
            return 0;
        std::unique_ptr<void, decltype(&mc_flex_chunk_free)> pending(chunk.owner, mc_flex_chunk_free);
        auto owner = std::make_shared<Owner>();
        owner->chunk = chunk;
        pending.release();
        const auto &c = owner->chunk;
        if (!c.data || !c.frames || !c.sample_rate || !c.channels ||
            c.length != size_t(c.frames) * c.channels || c.channels > std::numeric_limits<uint8_t>::max())
            throw std::runtime_error("Invalid flexaudio chunk layout");
        if (!anchor) {
            const int64_t first = RecordingTimeline::now_ns() - samples_to_ns(c.frames, c.sample_rate);
            arrival_offset = first - c.arrival_ns;
            anchor = first - c.media_ns;
        }
        const int64_t observed = c.arrival_ns + arrival_offset;
        owner->plane = reinterpret_cast<uint8_t *>(const_cast<float *>(c.data));
        owner->frame.emplace<MediaCap_AudioFrame>(
            MediaCap_AudioFrame{&owner->plane,
                                c.frames,
                                {MEDIACAP_SAMPLE_FORMAT_FLT, c.sample_rate, uint8_t(c.channels)},
                                *anchor + c.media_ns});
        if (m_frame_callback)
            m_frame_callback(std::shared_ptr<PackedFrame>(owner, &owner->frame), observed);
        return 1;
    };
    while (m_is_running.load()) {
        const int result = poll();
        if (result < 0)
            break;
        if (!result)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (mc_flex_stop(raw, &error) < 0)
        report_flex_error(m_errors, error);
    while (poll() > 0) {
    }
#else
    m_errors.report(MEDIACAP_ERROR_UNSUPPORTED, "flexaudio capture was disabled when MediaCap was built");
#endif
}
}
