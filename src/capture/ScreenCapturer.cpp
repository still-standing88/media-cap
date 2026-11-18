#include "ScreenCapturer.h"
#include "utils/RecordingTimeline.h"
#include <iostream>
#include <vector>
#include <array>

namespace MediaCap {

    struct OwningVideoFrame {
        std::shared_ptr<std::vector<uint8_t>> data_buffer;
        std::shared_ptr<std::array<uint8_t*, 4>> data_pointers;

        OwningVideoFrame(size_t buffer_size) {
            data_buffer = std::make_shared<std::vector<uint8_t>>(buffer_size);
            data_pointers = std::make_shared<std::array<uint8_t*, 4>>();
            (*data_pointers)[0] = data_buffer->data();
            (*data_pointers)[1] = nullptr;
            (*data_pointers)[2] = nullptr;
            (*data_pointers)[3] = nullptr;
        }
    };


    ScreenCapturer::ScreenCapturer(const MediaCap_Monitor& monitor, const MediaCap_ScreenCaptureConfig& config)
        : m_capture_source(monitor), m_capture_interval(config.capture_rate_ms) {
        m_source_type = MEDIACAP_SOURCE_TYPE_MONITOR;
    }

    ScreenCapturer::ScreenCapturer(const MediaCap_Window& window, const MediaCap_WindowCaptureConfig& config)
        : m_capture_source(window), m_capture_interval(config.capture_rate_ms) {
        m_source_type = MEDIACAP_SOURCE_TYPE_WINDOW;
    }

    ScreenCapturer::~ScreenCapturer() {
        stop();
    }

    void ScreenCapturer::start() {
        if (m_is_running.load()) return;
        { std::lock_guard<std::mutex> lock(m_diagnostic_mutex); m_last_operation.clear(); m_last_native = 0; }
        m_frame_index_counter.store(0);
        m_is_running.store(true);
        m_is_paused.store(false);
        m_capture_thread = std::thread([this] {
            try { capture_thread_func(); }
            catch (...) { m_errors.exception("Unexpected screen/window capture exception"); }
            m_is_running.store(false);
        });
    }

    void ScreenCapturer::stop() {
        m_is_running.store(false);
        if (m_capture_thread.joinable()) {
            m_capture_thread.join();
        }
        m_capture_manager.reset();
    }

    void ScreenCapturer::capture_error(bool retryable, const char* operation, int64_t native) {
        { std::lock_guard<std::mutex> lock(m_diagnostic_mutex); m_last_operation = operation; m_last_native = native; }
        if (!retryable) {
            m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, operation, native);
            m_is_running.store(false);
        }
    }

    void ScreenCapturer::report_startup_timeout() {
        std::string operation;
        int64_t native;
        { std::lock_guard<std::mutex> lock(m_diagnostic_mutex); operation = m_last_operation; native = m_last_native; }
        std::string message = "Timed out waiting for first screen/window frame";
        if (!operation.empty()) message += "; last backend failure: " + operation;
        m_errors.report(MEDIACAP_ERROR_TIMEOUT, message.c_str(), native);
    }

    void ScreenCapturer::capture_thread_func() {
        m_start_time = std::chrono::steady_clock::now();

        if (std::holds_alternative<MediaCap_Monitor>(m_capture_source)) {
            auto monitor = std::get<MediaCap_Monitor>(m_capture_source);
            m_capture_manager = SL::Screen_Capture::CreateCaptureConfiguration([this, monitor]() {
                try {
                auto monitors = SL::Screen_Capture::GetMonitors();
                for (const auto& m : monitors) {
                    if (m.Id == monitor.id) return std::vector<SL::Screen_Capture::Monitor>{m};
                }
                if (m_is_running.exchange(false)) m_errors.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Selected monitor is no longer available");
                } catch (...) { m_errors.exception("Monitor enumeration failed"); m_is_running.store(false); }
                return std::vector<SL::Screen_Capture::Monitor>{};
                })->onError([this](bool retryable, const char* operation, int64_t native) {
                    capture_error(retryable, operation, native);
                })->onNewFrame([this](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor&) {
                    this->on_new_frame(img);
                    })->start_capturing();

        }
        else if (std::holds_alternative<MediaCap_Window>(m_capture_source)) {
            auto window = std::get<MediaCap_Window>(m_capture_source);
            m_capture_manager = SL::Screen_Capture::CreateCaptureConfiguration([this, window]() {
                try {
                auto windows = SL::Screen_Capture::GetWindows();
                for (const auto& w : windows) {
                    if (w.Handle == window.handle) return std::vector<SL::Screen_Capture::Window>{w};
                }
                if (m_is_running.exchange(false)) m_errors.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Selected window is no longer available");
                } catch (...) { m_errors.exception("Window enumeration failed"); m_is_running.store(false); }
                return std::vector<SL::Screen_Capture::Window>{};
                })->onError([this](bool retryable, const char* operation, int64_t native) {
                    capture_error(retryable, operation, native);
                })->onNewFrame([this](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Window&) {
                    this->on_new_frame(img);
                    })->start_capturing();
        }

        if (!m_capture_manager) {
            m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Failed to start screen/window capture configuration");
            std::cerr << "MediaCap Error: Failed to start screen capture configuration." << std::endl;
            m_is_running.store(false);
            return;
        }

        m_capture_manager->setFrameChangeInterval(m_capture_interval);

        bool was_paused = false;
        while (m_is_running.load()) {
            bool is_paused_now = m_is_paused.load();
            if (is_paused_now != was_paused) {
                if (is_paused_now) m_capture_manager->pause();
                else m_capture_manager->resume();
                was_paused = is_paused_now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "MediaCap Info: Screen capture delivered " << m_frame_index_counter.load()
                  << " frame(s)." << std::endl;
    }

    void ScreenCapturer::on_new_frame(const SL::Screen_Capture::Image& img) try {
        const int64_t captured_at = RecordingTimeline::now_ns();
        if (!this->is_running() || this->is_paused() || !this->m_frame_callback) {
            return;
        }

        size_t data_size = SL::Screen_Capture::Width(img) * SL::Screen_Capture::Height(img) * sizeof(SL::Screen_Capture::ImageBGRA);

        auto owner = std::make_shared<OwningVideoFrame>(data_size);
        SL::Screen_Capture::Extract(img, owner->data_buffer->data(), data_size);

        auto* packed_frame_ptr = new PackedFrame();
        auto& frame_out = packed_frame_ptr->emplace<MediaCap_VideoFrame>();

        frame_out.width = SL::Screen_Capture::Width(img);
        frame_out.height = SL::Screen_Capture::Height(img);
        frame_out.pixel_format = MEDIACAP_PIXEL_FORMAT_BGRA;
        frame_out.orientation = MEDIACAP_ORIENTATION_TOP_DOWN;
        frame_out.timestamp_ns = captured_at;
        frame_out.frame_index = this->m_frame_index_counter.fetch_add(1);

        frame_out.data[0] = owner->data_buffer->data();
        frame_out.linesize[0] = static_cast<int32_t>(frame_out.width * sizeof(SL::Screen_Capture::ImageBGRA));
        for (int i = 1; i < 4; ++i) {
            frame_out.data[i] = nullptr;
            frame_out.linesize[i] = 0;
        }

        auto deleter = [owner](PackedFrame* p) {
            delete p;
            };
        std::shared_ptr<PackedFrame> frame_with_owner(packed_frame_ptr, deleter);

        this->m_frame_callback(frame_with_owner, -1);
    } catch (...) {
        m_errors.exception("Screen/window frame delivery failed"); m_is_running.store(false);
    }
}
