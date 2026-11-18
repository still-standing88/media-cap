#include "MediaCap_export.h"
#include "Discovery.h"
#include "utils/ErrorReporter.h"
#include "utils/Utf8.h"
#include "third_party/screen_capture_lite/include/ScreenCapture.h"
#include "third_party/CameraCapture/include/ccap.h"
#include "third_party/miniaudio/miniaudio.h"
#include <iostream>
#include <cstring> 

#if defined(_MSC_VER)
    #include <string.h> 
#else
    #include <cstring> 
#endif

namespace MediaCap::Native {

std::vector<Monitor> get_monitors() try {
    auto scl_monitors = SL::Screen_Capture::GetMonitors();
    std::vector<Monitor> monitors;
    monitors.reserve(scl_monitors.size());
    for (const auto& scl_mon : scl_monitors) {
        Monitor mon;
        mon.id = scl_mon.Id;
        mon.index = scl_mon.Index;
        mon.offsetX = scl_mon.OffsetX;
        mon.offsetY = scl_mon.OffsetY;
        mon.width = scl_mon.Width;
        mon.height = scl_mon.Height;
        mon.scaling = scl_mon.Scaling;
        Utf8::copy(mon.name, Utf8::bounded(scl_mon.Name));
        monitors.push_back(mon);
    }
    return monitors;
} catch (...) {
    Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).exception("Monitor discovery failed"); return {};
}

std::vector<Window> get_windows() try {
    auto scl_windows = SL::Screen_Capture::GetWindows();
    std::vector<Window> windows;
    windows.reserve(scl_windows.size());
    for (const auto& scl_win : scl_windows) {
        Window win;
        win.handle = scl_win.Handle;
        win.offsetX = scl_win.Position.x;
        win.offsetY = scl_win.Position.y;
        win.width = scl_win.Size.x;
        win.height = scl_win.Size.y;
        Utf8::copy(win.name, Utf8::bounded(scl_win.Name));
        windows.push_back(win);
    }
    return windows;
} catch (...) {
    Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).exception("Window discovery failed"); return {};
}

static MediaCap_PixelFormat from_ccap_format_discovery(ccap::PixelFormat format) {
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


std::vector<Camera> get_camera_devices() try {
    ccap::Provider provider;
    const auto names = provider.findDeviceNames();
    std::vector<Camera> cameras;
    for (size_t index = 0; index < names.size(); ++index) {
        Camera camera{};
        camera.index = static_cast<int>(index);
        Utf8::copy(camera.name, names[index]);
        cameras.push_back(camera);
    }
    return cameras;
} catch (...) {
    Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).exception("Camera discovery failed"); return {};
}

std::vector<CameraDetail> get_cameras() try {
    ccap::Provider provider_info;
    auto device_names = provider_info.findDeviceNames();
    std::vector<CameraDetail> cameras;
    int device_index = 0;
    for (const auto& name : device_names) {
        ccap::Provider provider_detail;
        if (!provider_detail.open(device_index, /*autoStart=*/false) || !provider_detail.isOpened()) {
            Errors::Reporter reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY);
            reporter.configure(nullptr, MEDIACAP_SOURCE_TYPE_CAMERA, device_index);
            reporter.report(MEDIACAP_ERROR_DEVICE_UNAVAILABLE, "Could not query camera capabilities");
            device_index++;
            continue;
        }
        auto ccap_info_opt = provider_detail.getDeviceInfo();
        if (ccap_info_opt) {
            CameraDetail cam_detail = {};
            cam_detail.camera.index = device_index;
            Utf8::copy(cam_detail.camera.name, name);
            
            cam_detail.supported_resolutions.reserve(ccap_info_opt->supportedResolutions.size());
            for (const auto& res : ccap_info_opt->supportedResolutions) {
                cam_detail.supported_resolutions.push_back({ res.width, res.height });
            }
            
            cam_detail.supported_pixel_formats.reserve(ccap_info_opt->supportedPixelFormats.size());
            for(const auto& fmt : ccap_info_opt->supportedPixelFormats) {
                cam_detail.supported_pixel_formats.push_back(from_ccap_format_discovery(fmt));
            }
            cameras.push_back(std::move(cam_detail));
        }
        provider_detail.close();
        device_index++;
    }
    return cameras;
} catch (...) {
    Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).exception("Camera discovery failed"); return {};
}


std::vector<AudioDevice> get_audio_devices() try {
    static_assert(sizeof(ma_device_id) <= MEDIACAP_MA_DEVICE_ID_SIZE, "MEDIACAP_MA_DEVICE_ID_SIZE is too small for ma_device_id");

    std::vector<AudioDevice> devices;
    ma_context context;
    const auto initialized = ma_context_init(NULL, 0, NULL, &context);
    if (initialized != MA_SUCCESS) {
        Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).report(MEDIACAP_ERROR_CAPTURE_FAILED,
            "Failed to initialize audio discovery", initialized);
        return devices;
    }
    struct ContextGuard { ma_context* context; ~ContextGuard() { ma_context_uninit(context); } } cleanup{&context};

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;

    const auto enumerated = ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount);
    if (enumerated != MA_SUCCESS) {
        Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).report(MEDIACAP_ERROR_CAPTURE_FAILED,
            "Failed to enumerate audio devices", enumerated);
        return devices;
    }

    for (ma_uint32 i = 0; i < captureCount; i++) {
        AudioDevice dev = {};
        memcpy(dev.id, &pCaptureInfos[i].id, sizeof(ma_device_id));
        Utf8::copy(dev.name, Utf8::bounded(pCaptureInfos[i].name));
        dev.type = MEDIACAP_AUDIO_DEVICE_TYPE_INPUT;
        dev.is_default = pCaptureInfos[i].isDefault;
        devices.push_back(dev);
    }

    if (ma_is_loopback_supported(context.backend)) {
         for (ma_uint32 i = 0; i < playbackCount; i++) {
            AudioDevice dev = {};
            memcpy(dev.id, &pPlaybackInfos[i].id, sizeof(ma_device_id));
            Utf8::copy(dev.name, Utf8::bounded(pPlaybackInfos[i].name));
            dev.type = MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK;
            dev.is_default = pPlaybackInfos[i].isDefault;
            devices.push_back(dev);
        }
    }
    
    return devices;
} catch (...) {
    Errors::Reporter(MEDIACAP_ERROR_COMPONENT_DISCOVERY).exception("Audio discovery failed"); return {};
}

}

