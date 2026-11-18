#ifndef MEDIACAP_AUDIOCAPTURER_H
#define MEDIACAP_AUDIOCAPTURER_H

#include "capture/Capturer.h"
#include "third_party/miniaudio/miniaudio.h"
#include <thread>
#include <chrono>
#include <array>
#include <condition_variable>
#include <mutex>

namespace MediaCap {

    class AudioCapturer : public Capturer {
    public:
        AudioCapturer(const MediaCap_AudioDevice& device, const MediaCap_AudioFormat& format,
            ma_context* context = nullptr);
        ~AudioCapturer() override;

        void start() override;
        void stop() override;

    private:
        bool init_device();
        void release_device();
        void capture_thread_func();

        static void ma_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
        static void ma_notification_callback(const ma_device_notification* notification);

        MediaCap_AudioDevice m_device_info;
        MediaCap_AudioFormat m_capture_format;
        ma_context* m_context = nullptr; // Optional caller-owned backend context; outlives this capturer.

        ma_device_config m_ma_device_config;
        ma_device m_ma_device;
        bool m_ma_device_initialized = false;
        ma_device m_loopback_silence;
        bool m_loopback_silence_initialized = false;

        std::thread m_capture_thread;
        struct OwnedBlock;
        struct Packet { std::shared_ptr<PackedFrame> frame; int64_t observation = -1; };
        static constexpr size_t pool_size = 64;
        std::vector<std::shared_ptr<OwnedBlock>> m_pool;
        std::array<Packet, pool_size + 1> m_ready;
        std::atomic<size_t> m_read{0}, m_write{0};
        std::atomic<uint64_t> m_overruns{0};
        std::atomic<bool> m_backend_stopped{false};
        std::mutex m_wake_mutex;
        std::condition_variable m_wake;
        size_t m_pool_cursor = 0, m_block_frames = 0;
        int64_t m_anchor_ns = 0, m_captured_frames = 0;
        bool m_clock_started = false;
    };

}

#endif
