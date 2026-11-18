#include "CameraCapturer.h"
#include "utils/RecordingTimeline.h"
#include <optional>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace MediaCap {

    static void close_provider_with_timeout(ccap::Provider& provider, const std::string& camera_name, const Errors::Reporter& errors) {
        auto done = std::make_shared<std::atomic<bool>>(false);
        auto failure = std::make_shared<std::exception_ptr>();
        auto provider_holder = std::make_shared<ccap::Provider>(std::move(provider));
        std::thread([provider_holder, done, failure]() {
            try { provider_holder->close(); } catch (...) { *failure = std::current_exception(); }
            done->store(true);
            }).detach();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!done->load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!done->load()) {
            errors.report(MEDIACAP_ERROR_TIMEOUT, "Camera backend cleanup timed out");
            std::cerr << "MediaCap Error: Camera '" << camera_name
                       << "' did not close within 3s (stuck DirectShow/Media Foundation call) - abandoning it so the recording can finish." << std::endl;
        } else if (*failure) {
            try { std::rethrow_exception(*failure); } catch (...) { errors.exception("Camera backend cleanup failed"); }
        }
    }

    static MediaCap_PixelFormat from_ccap_format(ccap::PixelFormat format) {
        switch (format) {
        case ccap::PixelFormat::BGRA32: return MEDIACAP_PIXEL_FORMAT_BGRA;
        case ccap::PixelFormat::BGR24:  return MEDIACAP_PIXEL_FORMAT_BGR24;
        case ccap::PixelFormat::RGBA32: return MEDIACAP_PIXEL_FORMAT_RGBA;
        case ccap::PixelFormat::RGB24:  return MEDIACAP_PIXEL_FORMAT_RGB24;
        case ccap::PixelFormat::NV12:   return MEDIACAP_PIXEL_FORMAT_NV12;
        case ccap::PixelFormat::I420:   return MEDIACAP_PIXEL_FORMAT_I420;
        default: return MEDIACAP_PIXEL_FORMAT_UNKNOWN;
        }
    }

    static MediaCap_FrameOrientation from_ccap_orientation(ccap::FrameOrientation orientation) {
        return (orientation == ccap::FrameOrientation::BottomToTop) ? MEDIACAP_ORIENTATION_BOTTOM_UP : MEDIACAP_ORIENTATION_TOP_DOWN;
    }

    static ccap::PixelFormat requested_format(MediaCap_PixelFormat format) {
        switch (format) {
        case MEDIACAP_PIXEL_FORMAT_UNKNOWN:
        case MEDIACAP_PIXEL_FORMAT_BGRA: return ccap::PixelFormat::BGRA32;
        case MEDIACAP_PIXEL_FORMAT_BGR24: return ccap::PixelFormat::BGR24;
        case MEDIACAP_PIXEL_FORMAT_RGBA: return ccap::PixelFormat::RGBA32;
        case MEDIACAP_PIXEL_FORMAT_RGB24: return ccap::PixelFormat::RGB24;
        case MEDIACAP_PIXEL_FORMAT_NV12: return ccap::PixelFormat::NV12;
        case MEDIACAP_PIXEL_FORMAT_I420: return ccap::PixelFormat::I420;
        default: throw std::invalid_argument("Unknown camera pixel format");
        }
    }

    CameraCapturer::CameraCapturer(const MediaCap_Camera& camera, const MediaCap_CameraCaptureConfig& config)
        : m_camera_info(camera), m_capture_config(config) {
        m_source_type = MEDIACAP_SOURCE_TYPE_CAMERA;
    }

    CameraCapturer::~CameraCapturer() {
        stop();
    }

    void CameraCapturer::start() {
        if (m_is_running.load()) return;
        m_is_running.store(true);
        m_is_paused.store(false);
        m_capture_thread = std::thread([this] {
            try { capture_thread_func(); }
            catch (...) {
                m_errors.exception("Unexpected camera capture exception");
                try { close_provider_with_timeout(m_provider, m_camera_info.name, m_errors); }
                catch (...) { m_errors.exception("Camera cleanup failed"); }
            }
            m_is_running.store(false);
        });
    }

    void CameraCapturer::stop() {
        m_is_running.store(false);
        if (m_capture_thread.joinable()) {
            m_capture_thread.join();
        }
    }

    void CameraCapturer::capture_thread_func() {
        std::optional<int64_t> clock_offset;
        int64_t previous_backend_time = -1;
        m_provider.set(ccap::PropertyName::Width, static_cast<double>(m_capture_config.width));
        m_provider.set(ccap::PropertyName::Height, static_cast<double>(m_capture_config.height));
        m_provider.set(ccap::PropertyName::FrameRate, static_cast<double>(m_capture_config.framerate));
        const auto output_format = requested_format(m_capture_config.pixel_format);
        m_provider.set(ccap::PropertyName::PixelFormatOutput, output_format);

        if (!m_provider.open(m_camera_info.index, true)) {
            m_errors.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Failed to open the selected camera");
            std::cerr << "MediaCap Error: Failed to open camera '" << m_camera_info.name << "'." << std::endl;
            m_is_running.store(false);
            return;
        }

        if (!m_provider.isStarted()) {
            m_errors.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Camera opened but failed to start");
            std::cerr << "MediaCap Error: Camera '" << m_camera_info.name << "' opened but failed to start." << std::endl;
            close_provider_with_timeout(m_provider, m_camera_info.name, m_errors);
            m_is_running.store(false);
            return;
        }

        std::cout << "MediaCap Info: Camera '" << m_camera_info.name << "' started successfully." << std::endl;

        while (m_is_running.load()) {
            if (m_is_paused.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            auto ccap_frame = m_provider.grab(1000);
            if (!ccap_frame) {
                if (!m_provider.isStarted()) {
                    m_errors.report(MEDIACAP_ERROR_DEVICE_LOST, "Camera backend stopped unexpectedly");
                    std::cerr << "MediaCap Error: Camera '" << m_camera_info.name << "' stopped unexpectedly." << std::endl;
                    m_is_running.store(false);
                    break;
                }
                continue;
            }
            if (ccap_frame->pixelFormat != output_format) {
                m_errors.report(MEDIACAP_ERROR_UNSUPPORTED, "Video source cannot deliver the requested camera pixel format");
                break;
            }
            if (m_frame_callback) {
                auto packed_frame = new PackedFrame();
                auto& frame_out = packed_frame->emplace<MediaCap_VideoFrame>();

                frame_out.width = ccap_frame->width;
                frame_out.height = ccap_frame->height;
                frame_out.pixel_format = from_ccap_format(ccap_frame->pixelFormat);
                frame_out.orientation = from_ccap_orientation(ccap_frame->orientation);
                const int64_t backend_time = static_cast<int64_t>(ccap_frame->timestamp);
                if (!clock_offset || backend_time < previous_backend_time)
                    clock_offset = RecordingTimeline::now_ns() - backend_time;
                frame_out.timestamp_ns = backend_time + *clock_offset;
                previous_backend_time = backend_time;
                frame_out.frame_index = ccap_frame->frameIndex;

                for (int i = 0; i < 3; ++i) {
                    frame_out.data[i] = ccap_frame->data[i];
                    frame_out.linesize[i] = ccap_frame->stride[i];
                }

                auto deleter = [ccap_frame](PackedFrame* p) {
                    delete p;
                    };
                std::shared_ptr<PackedFrame> frame_with_owner(packed_frame, deleter);

                m_frame_callback(frame_with_owner, -1);
            }
        }

        close_provider_with_timeout(m_provider, m_camera_info.name, m_errors);
        std::cout << "MediaCap Info: Camera '" << m_camera_info.name << "' stopped." << std::endl;
    }

}
