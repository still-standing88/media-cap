#include "MediaCap_c.h"
#include "core/Session.h"
#include "discovery/Discovery.h"
#include "capture/Screenshot.h"
#include "capture/ScreenCapturer.h"
#include "utils/ErrorReporter.h"
#include "integrations/FFmpegMuxer.h"
#include <vector>
#include <new>
#include <algorithm>
#include <cstring>

static MediaCap::Native::Session* to_session(MediaCap_Session_Handle handle) {
    return reinterpret_cast<MediaCap::Native::Session*>(handle);
}

extern "C" MEDIACAP_API int MediaCap_ValidateRecordingFormat(const MediaCap_RecordingConfig* config) {
    const MediaCap::Errors::Reporter reporter(MEDIACAP_ERROR_COMPONENT_ENCODER);
    try {
        if (config) return MediaCap::FFmpegMuxer::validate_format(*config, reporter) ? 1 : 0;
        reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Missing recording configuration");
    } catch (...) {
        reporter.exception("Could not validate the recording format");
    }
    return 0;
}
extern "C" MEDIACAP_API int MediaCap_Session_AddAudioCaptureSource(MediaCap_Session_Handle handle,const MediaCap_AudioSourceConfig* config) {
    if(handle&&config)return to_session(handle)->add_audio_source(*config);
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT,"Audio capture requires a session and configuration");
    return 0;
}

extern "C" MEDIACAP_API MediaCap_Session_Handle MediaCap_Session_Create() try {
    try {
        return reinterpret_cast<MediaCap_Session_Handle>(new MediaCap::Native::Session());
    }
    catch (const std::bad_alloc&) {
        MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Session allocation failed");
        return nullptr;
    }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_Create failed");
    return nullptr;
}

extern "C" MEDIACAP_API void MediaCap_Session_Destroy(MediaCap_Session_Handle handle) try {
    delete to_session(handle);
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_Destroy failed");
    return;
}

extern "C" MEDIACAP_API int MediaCap_GetMonitors(MediaCap_Monitor* buffer, int* count) try {
    if (!count || *count < 0) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid device count argument"); return -1; }
    try {
        auto monitors = MediaCap::Native::get_monitors();
        if (!buffer || *count == 0) {
            *count = static_cast<int>(monitors.size());
        }
        else {
            int copy_count = std::min(*count, static_cast<int>(monitors.size()));
            memcpy(buffer, monitors.data(), copy_count * sizeof(MediaCap::Monitor));
            *count = copy_count;
        }
        return 0;
    }
    catch (...) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Device enumeration failed"); return -1; }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_GetMonitors failed");
    return -1;
}

extern "C" MEDIACAP_API int MediaCap_GetWindows(MediaCap_Window* buffer, int* count) try {
    if (!count || *count < 0) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid device count argument"); return -1; }
    try {
        auto windows = MediaCap::Native::get_windows();
        if (!buffer || *count == 0) {
            *count = static_cast<int>(windows.size());
        }
        else {
            int copy_count = std::min(*count, static_cast<int>(windows.size()));
            memcpy(buffer, windows.data(), copy_count * sizeof(MediaCap::Window));
            *count = copy_count;
        }
        return 0;
    }
    catch (...) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Device enumeration failed"); return -1; }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_GetWindows failed");
    return -1;
}

extern "C" MEDIACAP_API int MediaCap_GetCameraDevices(MediaCap_Camera* buffer, int* count) try {
    if (!count || *count < 0) {
        MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid device count argument");
        return -1;
    }
    const auto cameras = MediaCap::Native::get_camera_devices();
    const int copied = buffer && *count ? std::min(*count, static_cast<int>(cameras.size())) : static_cast<int>(cameras.size());
    if (buffer && *count && copied) memcpy(buffer, cameras.data(), copied * sizeof(MediaCap_Camera));
    *count = copied;
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Camera discovery failed"); return -1;
}

extern "C" MEDIACAP_API int MediaCap_GetCameras(MediaCap_CameraInfo* buffer, int* count) try {
    if (!count || *count < 0) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid device count argument"); return -1; }
    try {
        auto cameras = MediaCap::Native::get_cameras();
        if (!buffer || *count == 0) {
            *count = static_cast<int>(cameras.size());
        }
        else {
            int copy_count = std::min(*count, static_cast<int>(cameras.size()));
            for (int i = 0; i < copy_count; ++i) {
                const auto& cam_detail = cameras[i];
                buffer[i].camera = cam_detail.camera;

                size_t num_resolutions = cam_detail.supported_resolutions.size();
                buffer[i].supported_resolutions = new (std::nothrow) MediaCap_Resolution[num_resolutions];
                if (buffer[i].supported_resolutions) {
                    memcpy(buffer[i].supported_resolutions, cam_detail.supported_resolutions.data(), num_resolutions * sizeof(MediaCap_Resolution));
                    buffer[i].num_supported_resolutions = num_resolutions;
                }
                else {
                    buffer[i].num_supported_resolutions = 0;
                }

                size_t num_pixel_formats = cam_detail.supported_pixel_formats.size();
                buffer[i].supported_pixel_formats = new (std::nothrow) MediaCap_PixelFormat[num_pixel_formats];
                if (buffer[i].supported_pixel_formats) {
                    memcpy(buffer[i].supported_pixel_formats, cam_detail.supported_pixel_formats.data(), num_pixel_formats * sizeof(MediaCap_PixelFormat));
                    buffer[i].num_supported_pixel_formats = num_pixel_formats;
                }
                else {
                    buffer[i].num_supported_pixel_formats = 0;
                }
            }
            *count = copy_count;
        }
        return 0;
    }
    catch (...) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Device enumeration failed"); return -1; }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_GetCameras failed");
    return -1;
}

extern "C" MEDIACAP_API void MediaCap_FreeCameraInfoContents(MediaCap_CameraInfo* info) try {
    if (info) {
        delete[] info->supported_resolutions;
        info->supported_resolutions = nullptr;
        delete[] info->supported_pixel_formats;
        info->supported_pixel_formats = nullptr;
    }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_FreeCameraInfoContents failed");
    return;
}

extern "C" MEDIACAP_API int MediaCap_GetAudioDevices(MediaCap_AudioDevice* buffer, int* count) try {
    if (!count || *count < 0) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid device count argument"); return -1; }
    try {
        auto devices = MediaCap::Native::get_audio_devices();
        if (!buffer || *count == 0) {
            *count = static_cast<int>(devices.size());
        }
        else {
            int copy_count = std::min(*count, static_cast<int>(devices.size()));
            memcpy(buffer, devices.data(), copy_count * sizeof(MediaCap::AudioDevice));
            *count = copy_count;
        }
        return 0;
    }
    catch (...) { MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Device enumeration failed"); return -1; }
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_GetAudioDevices failed");
    return -1;
}

extern "C" MEDIACAP_API void MediaCap_Session_AddMonitorSource(MediaCap_Session_Handle handle, const MediaCap_Monitor* monitor, const MediaCap_ScreenCaptureConfig* config) try {
    if (handle && monitor && config) to_session(handle)->add_source(*monitor, *config);
    else MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "AddSource requires a session, source, and configuration");
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_AddMonitorSource failed");
    return;
}

extern "C" MEDIACAP_API void MediaCap_Session_AddWindowSource(MediaCap_Session_Handle handle, const MediaCap_Window* window, const MediaCap_WindowCaptureConfig* config) try {
    if (handle && window && config) to_session(handle)->add_source(*window, *config);
    else MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "AddSource requires a session, source, and configuration");
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_AddWindowSource failed");
    return;
}

extern "C" MEDIACAP_API void MediaCap_Session_AddCameraSource(MediaCap_Session_Handle handle, const MediaCap_Camera* camera, const MediaCap_CameraCaptureConfig* config) try {
    if (handle && camera && config) to_session(handle)->add_source(*camera, *config);
    else MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "AddSource requires a session, source, and configuration");
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_AddCameraSource failed");
    return;
}

extern "C" MEDIACAP_API void MediaCap_Session_AddAudioSource(MediaCap_Session_Handle handle, const MediaCap_AudioDevice* device, const MediaCap_AudioFormat* format) try {
    if (handle && device && format) to_session(handle)->add_source(*device, *format);
    else MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "AddSource requires a session, source, and configuration");
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_AddAudioSource failed");
    return;
}

extern "C" MEDIACAP_API int MediaCap_Session_SetFrameCallbacks(MediaCap_Session_Handle handle,
    MediaCap_AudioFrameCallback audio_callback, MediaCap_VideoFrameCallback video_callback, void* user_data) try {
    return handle && to_session(handle)->set_frame_callbacks(audio_callback, video_callback, user_data);
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not set raw frame callbacks");
    return 0;
}

extern "C" MEDIACAP_API int MediaCap_Session_StartCapture(MediaCap_Session_Handle handle) try {
    if (handle) return to_session(handle)->start_capture();
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(
        MEDIACAP_ERROR_INVALID_ARGUMENT, "Capture requires a session handle");
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not start capture");
    return 0;
}

extern "C" MEDIACAP_API void MediaCap_Session_StopCapture(MediaCap_Session_Handle handle) try {
    if (handle) to_session(handle)->stop_capture();
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not stop capture");
}

extern "C" MEDIACAP_API int MediaCap_Session_IsCapturing(MediaCap_Session_Handle handle) {
    return handle && to_session(handle)->is_capturing();
}

extern "C" MEDIACAP_API int MediaCap_CaptureMonitorScreenshot(const MediaCap_Monitor* monitor,
    const MediaCap_ScreenshotConfig* config, MediaCap_ScreenshotCallback callback, void* user_data) try {
    if (monitor) {
        auto source = std::make_unique<MediaCap::ScreenCapturer>(*monitor, MediaCap_ScreenCaptureConfig{16});
        return MediaCap::capture_screenshot(std::move(source), config ? *config : MediaCap_ScreenshotConfig{}, callback, user_data);
    }
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(
        MEDIACAP_ERROR_INVALID_ARGUMENT, "Screenshot requires a monitor");
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not capture a monitor screenshot");
    return 0;
}

extern "C" MEDIACAP_API int MediaCap_CaptureWindowScreenshot(const MediaCap_Window* window,
    const MediaCap_ScreenshotConfig* config, MediaCap_ScreenshotCallback callback, void* user_data) try {
    if (window) {
        auto source = std::make_unique<MediaCap::ScreenCapturer>(*window, MediaCap_WindowCaptureConfig{16});
        return MediaCap::capture_screenshot(std::move(source), config ? *config : MediaCap_ScreenshotConfig{}, callback, user_data);
    }
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(
        MEDIACAP_ERROR_INVALID_ARGUMENT, "Screenshot requires a window");
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not capture a window screenshot");
    return 0;
}

extern "C" MEDIACAP_API int MediaCap_Session_StartRecording(MediaCap_Session_Handle handle, const MediaCap_RecordingConfig* config) try {
    if (handle && config) return to_session(handle)->start_recording(*config) ? 1 : 0;
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "StartRecording requires a session and configuration");
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_StartRecording failed");
    return 0;
}

extern "C" MEDIACAP_API int MediaCap_Session_StartRecordingForDuration(MediaCap_Session_Handle handle, const MediaCap_RecordingConfig* config, long long duration_ms) try {
    if (handle && config) return to_session(handle)->start_recording(*config, std::chrono::milliseconds(duration_ms)) ? 1 : 0;
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT, "StartRecording requires a session and configuration");
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_StartRecordingForDuration failed");
    return 0;
}

extern "C" MEDIACAP_API void MediaCap_Session_StopRecording(MediaCap_Session_Handle handle) try {
    if (handle) to_session(handle)->stop_recording();
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_StopRecording failed");
    return;
}

extern "C" MEDIACAP_API void MediaCap_Session_SetPaused(MediaCap_Session_Handle handle, int is_paused) try {
    if (handle) to_session(handle)->set_paused(is_paused != 0);
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_SetPaused failed");
    return;
}

extern "C" MEDIACAP_API int MediaCap_Session_IsRecording(MediaCap_Session_Handle handle) try {
    if (handle) return to_session(handle)->is_recording() ? 1 : 0;
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_IsRecording failed");
    return 0;
}

extern "C" MEDIACAP_API int MediaCap_Session_IsPaused(MediaCap_Session_Handle handle) try {
    if (handle) return to_session(handle)->is_paused() ? 1 : 0;
    return 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_IsPaused failed");
    return 0;
}

extern "C" MEDIACAP_API uint64_t MediaCap_Session_GetId(MediaCap_Session_Handle handle) try {
    return handle ? to_session(handle)->id() : 0;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_GetId failed");
    return 0;
}
extern "C" MEDIACAP_API int MediaCap_Session_HasError(MediaCap_Session_Handle handle) try {
    return handle && to_session(handle)->has_error();
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_HasError failed");
    return 0;
}
extern "C" MEDIACAP_API int MediaCap_Session_GetError(MediaCap_Session_Handle handle, MediaCap_Error* error) try {
    if (!error) return 0;
    *error = handle ? to_session(handle)->get_error() : MediaCap_Error{};
    return error->code != MEDIACAP_ERROR_NONE;
} catch (...) {
    MediaCap::Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("MediaCap_Session_GetError failed");
    return 0;
}
