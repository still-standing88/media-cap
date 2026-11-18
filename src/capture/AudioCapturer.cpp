#include "AudioCapturer.h"
#include "utils/RecordingTimeline.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace MediaCap {
struct AudioCapturer::OwnedBlock {
    std::vector<uint8_t> data;
    uint8_t* plane = nullptr;
    PackedFrame frame;
};
namespace {
ma_format to_ma_format(MediaCap_SampleFormat format) {
    switch (format) {
    case MEDIACAP_SAMPLE_FORMAT_U8: return ma_format_u8;
    case MEDIACAP_SAMPLE_FORMAT_S16: return ma_format_s16;
    case MEDIACAP_SAMPLE_FORMAT_S32: return ma_format_s32;
    case MEDIACAP_SAMPLE_FORMAT_FLT: return ma_format_f32;
    default: return ma_format_unknown;
    }
}
}
AudioCapturer::AudioCapturer(const MediaCap_AudioDevice& device, const MediaCap_AudioFormat& format, ma_context* context)
    : m_device_info(device), m_capture_format(format), m_context(context) {
    if (device.type == MEDIACAP_AUDIO_DEVICE_TYPE_INPUT) m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_INPUT;
    else if (device.type == MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK) m_source_type = MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT;
}
AudioCapturer::~AudioCapturer() { stop(); }
void AudioCapturer::start() {
    if (m_is_running.exchange(true)) return;
    m_backend_stopped.store(false);
    m_is_paused.store(false);
    m_capture_thread = std::thread(&AudioCapturer::capture_thread_func, this);
}
void AudioCapturer::stop() {
    m_is_running.store(false); m_wake.notify_one();
    if (m_capture_thread.joinable()) m_capture_thread.join();
}
bool AudioCapturer::init_device() {
    if (!m_capture_format.sample_rate || !m_capture_format.channels ||
        to_ma_format(m_capture_format.format) == ma_format_unknown) {
        m_errors.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Unsupported audio capture format");
        return false;
    }
    ma_device_type type;
    if (m_source_type == MEDIACAP_SOURCE_TYPE_AUDIO_INPUT) type = ma_device_type_capture;
    else if (m_source_type == MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT) type = ma_device_type_loopback;
    else { m_errors.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Unsupported audio source type"); return false; }
    m_ma_device_config = ma_device_config_init(type);
    m_ma_device_config.capture.pDeviceID = reinterpret_cast<const ma_device_id*>(m_device_info.id);
    m_ma_device_config.capture.format = to_ma_format(m_capture_format.format);
    m_ma_device_config.capture.channels = m_capture_format.channels;
    m_ma_device_config.sampleRate = m_capture_format.sample_rate;
    m_ma_device_config.dataCallback = &AudioCapturer::ma_data_callback;
    m_ma_device_config.notificationCallback = &AudioCapturer::ma_notification_callback;
    m_ma_device_config.pUserData = this;
    const auto initialized = ma_device_init(m_context, &m_ma_device_config, &m_ma_device);
    if (initialized != MA_SUCCESS) {
        if (initialized == MA_FORMAT_NOT_SUPPORTED || initialized == MA_INVALID_DEVICE_CONFIG)
            m_errors.report(MEDIACAP_ERROR_UNSUPPORTED,
                "Audio device cannot use the requested sample format, rate, or channel count", initialized);
        else
            m_errors.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Failed to initialize audio device", initialized);
        return false;
    }
    m_ma_device_initialized = true;
    if (type == ma_device_type_loopback && m_ma_device.pContext->backend == ma_backend_wasapi) {
        auto silence = ma_device_config_init(ma_device_type_playback);
        silence.playback.pDeviceID = m_ma_device_config.capture.pDeviceID;
        silence.playback.format = ma_format_f32;
        silence.playback.channels = m_capture_format.channels;
        silence.sampleRate = m_capture_format.sample_rate;
        silence.pUserData = this;
        silence.notificationCallback = &AudioCapturer::ma_notification_callback;
        silence.dataCallback = [](ma_device* device, void* output, const void*, ma_uint32 frames) {
            ma_silence_pcm_frames(output, frames, device->playback.format, device->playback.channels);
        };
        const auto silence_initialized = ma_device_init(m_ma_device.pContext, &silence, &m_loopback_silence);
        if (silence_initialized != MA_SUCCESS) {
            m_errors.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Failed to initialize loopback keepalive", silence_initialized);
            return false;
        }
        m_loopback_silence_initialized = true;
        const auto silence_started = ma_device_start(&m_loopback_silence);
        if (silence_started != MA_SUCCESS) {
            m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Failed to start loopback keepalive", silence_started);
            return false;
        }
    }
    m_block_frames = std::max<size_t>(4096, m_capture_format.sample_rate / 10);
    const size_t bytes = m_block_frames * m_capture_format.channels *
        ma_get_bytes_per_sample(to_ma_format(m_capture_format.format));
    m_pool.clear(); m_pool.reserve(pool_size);
    for (size_t i = 0; i < pool_size; ++i) {
        auto owner = std::make_shared<OwnedBlock>();
        owner->data.resize(bytes); owner->plane = owner->data.data();
        owner->frame.emplace<MediaCap_AudioFrame>(MediaCap_AudioFrame{
            &owner->plane, 0, m_capture_format, 0});
        m_pool.push_back(std::move(owner));
    }
    m_clock_started = false; m_captured_frames = 0;
    m_read.store(0); m_write.store(0); m_overruns.store(0);
    return true;
}
void AudioCapturer::release_device() {
    if (m_loopback_silence_initialized) { ma_device_uninit(&m_loopback_silence); m_loopback_silence_initialized = false; }
    if (m_ma_device_initialized) { ma_device_uninit(&m_ma_device); m_ma_device_initialized = false; }
}
void AudioCapturer::capture_thread_func() {
    bool reported_overrun = false;
    const auto drain = [&] {
        auto read = m_read.load(std::memory_order_relaxed);
        while (read != m_write.load(std::memory_order_acquire)) {
            Packet packet = std::move(m_ready[read]);
            read = (read + 1) % m_ready.size();
            m_read.store(read, std::memory_order_release);
            if (m_frame_callback) m_frame_callback(std::move(packet.frame), packet.observation);
        }
    };
    try {
        if (init_device() && m_is_running.load()) {
            const auto started = ma_device_start(&m_ma_device);
            if (started == MA_SUCCESS) {
                while (m_is_running.load()) {
                    if (m_backend_stopped.load()) {
                        m_errors.report(MEDIACAP_ERROR_DEVICE_LOST, "Audio backend stopped unexpectedly"); break;
                    }
                    if (m_overruns.load() && !reported_overrun) {
                        reported_overrun = true;
                        m_errors.report(MEDIACAP_ERROR_BUFFER_OVERFLOW, "Audio capture buffer overflow; samples were lost");
                    }
                    drain();
                    std::unique_lock<std::mutex> lock(m_wake_mutex);
                    m_wake.wait_for(lock, std::chrono::milliseconds(10), [&] {
                        return !m_is_running.load() || m_read.load() != m_write.load();
                    });
                }
                ma_device_stop(&m_ma_device);
                drain();
            } else m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Failed to start audio device", started);
        }
    } catch (const std::exception& error) {
        m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, error.what());
    } catch (...) {
        m_errors.exception("Unexpected audio capture exception");
    }
    m_is_running.store(false);
    release_device();
    if (m_overruns.load() && !reported_overrun)
        m_errors.report(MEDIACAP_ERROR_BUFFER_OVERFLOW, "Audio capture buffer overflow; samples were lost");
}
void AudioCapturer::ma_notification_callback(const ma_device_notification* notification) {
    auto* self = static_cast<AudioCapturer*>(notification->pDevice->pUserData);
    if (self && self->m_is_running.load() && notification->type == ma_device_notification_type_stopped) {
        self->m_backend_stopped.store(true); self->m_wake.notify_one();
    }
}
void AudioCapturer::ma_data_callback(ma_device* device, void*, const void* input, ma_uint32 frames) {
    auto* self = static_cast<AudioCapturer*>(device->pUserData);
    if (!self || !self->m_is_running.load() || !input || !frames) return;
    const int64_t observed_end = RecordingTimeline::now_ns();
    const uint32_t rate = self->m_capture_format.sample_rate;
    if (!self->m_clock_started) {
        self->m_anchor_ns = observed_end - samples_to_ns(frames, rate);
        self->m_clock_started = true;
    }
    const int64_t first = self->m_captured_frames;
    self->m_captured_frames += frames;
    const size_t stride = self->m_capture_format.channels *
        ma_get_bytes_per_sample(to_ma_format(self->m_capture_format.format));
    for (size_t offset = 0; offset < frames;) {
        const size_t count = std::min(self->m_block_frames, size_t(frames) - offset);
        const size_t write = self->m_write.load(std::memory_order_relaxed);
        const size_t next = (write + 1) % self->m_ready.size();
        std::shared_ptr<OwnedBlock>* available = nullptr;
        if (next != self->m_read.load(std::memory_order_acquire)) {
            for (size_t n = 0; n < pool_size; ++n) {
                auto& block = self->m_pool[(self->m_pool_cursor++) % pool_size];
                if (block.use_count() == 1) { available = &block; break; }
            }
        }
        if (available) {
            auto& owner = *available;
            std::memcpy(owner->data.data(), static_cast<const uint8_t*>(input) + offset * stride, count * stride);
            auto& frame = std::get<MediaCap_AudioFrame>(owner->frame);
            frame.num_samples = uint32_t(count);
            frame.timestamp_ns = self->m_anchor_ns + samples_to_ns(first + offset, rate);
            self->m_ready[write] = {std::shared_ptr<PackedFrame>(owner, &owner->frame),
                observed_end - samples_to_ns(frames - offset, rate)};
            self->m_write.store(next, std::memory_order_release);
            self->m_wake.notify_one();
        } else self->m_overruns.fetch_add(count);
        offset += count;
    }
}
}
