#ifndef MEDIACAP_SCREENCAPTURER_H
#define MEDIACAP_SCREENCAPTURER_H

#include "capture/Capturer.h"
#include <variant>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100)
#endif
#include "third_party/screen_capture_lite/include/ScreenCapture.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <thread>
#include <variant>
#include <chrono>

namespace MediaCap {

    using ScreenCaptureSource = std::variant<MediaCap_Monitor, MediaCap_Window>;

    class ScreenCapturer : public Capturer {
    public:
        ScreenCapturer(const MediaCap_Monitor& monitor, const MediaCap_ScreenCaptureConfig& config);
        ScreenCapturer(const MediaCap_Window& window, const MediaCap_WindowCaptureConfig& config);
        ~ScreenCapturer() override;

        void start() override;
        void stop() override;
        void report_startup_timeout() override;

    private:
        void capture_error(bool retryable, const char* operation, int64_t native);
        std::mutex m_diagnostic_mutex;
        std::string m_last_operation;
        int64_t m_last_native = 0;
        void capture_thread_func();
        void on_new_frame(const SL::Screen_Capture::Image& img);

        ScreenCaptureSource m_capture_source;
        std::chrono::milliseconds m_capture_interval;

        std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> m_capture_manager;
        std::thread m_capture_thread;
        std::chrono::steady_clock::time_point m_start_time;
        std::atomic<uint64_t> m_frame_index_counter{ 0 };
    };

}

#endif
