// Test-only public API implementation. Never linked into the installed CLI.
#include <MediaCap_c.h>
#include <MediaCap_audio.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>
struct MediaCap_Session_t {
    MediaCap_AudioFrameCallback audio = nullptr;
    MediaCap_VideoFrameCallback video = nullptr;
    void* user_data = nullptr;
    MediaCap_SourceType video_source = MEDIACAP_SOURCE_TYPE_UNKNOWN;
    bool audio_added = false, capturing = false, recording = false, paused = false;
};
namespace {
MediaCap_Error last{};
std::string mode() { const char* s = std::getenv("MC_TEST_SCENARIO"); return s ? s : "normal"; }
void fail(MediaCap_ErrorCode code = MEDIACAP_ERROR_CAPTURE_FAILED, MediaCap_ErrorComponent component = MEDIACAP_ERROR_COMPONENT_DISCOVERY) {
    last = {}; last.code = code; last.component = component;
    std::strcpy(last.message, "INTERNAL diagnostic from capture library: fixture failure");
}
bool query(const char* name) {
    std::fputs("INTERNAL stdout diagnostic\n", stdout);
    std::fputs("INTERNAL stderr diagnostic\n", stderr);
    if (mode() == "error" || (mode() == "windows-error" && std::string(name) == "windows")) { fail(); return false; }
    return mode() != "empty";
}
char* owned(const char* s) { auto* p = new char[std::strlen(s) + 1]; std::strcpy(p, s); return p; }

MediaCap_VideoFrameView video_frame() {
    static const uint8_t pixels[]{1, 2, 3, 255, 4, 5, 6, 255, 99, 99, 99, 99,
        7, 8, 9, 255, 10, 11, 12, 255, 99, 99, 99, 99};
    return {{pixels}, {12}, 2, 2, MEDIACAP_PIXEL_FORMAT_BGRA, MEDIACAP_ORIENTATION_TOP_DOWN, 1010, 4};
}

void deliver_frames(MediaCap_Session_Handle session) {
    const bool video = session->video_source != MEDIACAP_SOURCE_TYPE_UNKNOWN;
    if (video && session->video) {
        auto frame = video_frame();
        if (mode() == "bad-frame") frame.linesize[0] = 1;
        session->video(0, session->video_source, &frame, session->user_data);
    }
    if (session->audio_added && session->audio) {
        static const int16_t samples[]{1, 2, 3, 4};
        const MediaCap_AudioFrameView frame{reinterpret_cast<const uint8_t*>(samples), 2,
            {MEDIACAP_SAMPLE_FORMAT_S16, 48000, 2}, 2020};
        session->audio(video ? 1 : 0, MEDIACAP_SOURCE_TYPE_AUDIO_INPUT, &frame, session->user_data);
    }
}

int screenshot(const MediaCap_ScreenshotConfig* config, MediaCap_ScreenshotCallback callback, void* data) {
    if (mode() == "screenshot-error") {
        fail(MEDIACAP_ERROR_TIMEOUT, MEDIACAP_ERROR_COMPONENT_CAPTURE);
        return 0;
    }
    if (mode() == "screenshot-config" && (!config || config->x != 1 || config->y != 2 || config->width != 3 ||
        config->height != 4 || config->output_width != 2 || config->output_height != 2 || config->timeout_ms != 456)) {
        fail(MEDIACAP_ERROR_INVALID_ARGUMENT, MEDIACAP_ERROR_COMPONENT_CAPTURE);
        return 0;
    }
    const auto frame = video_frame();
    callback(&frame, data);
    return 1;
}
}
extern "C" {
int MediaCap_GetMonitors(MediaCap_Monitor* buffer, int* count) {
    if (!query("monitors")) { *count = 0; return 0; }
    if (buffer && *count) { *buffer = {}; buffer->id = 7; buffer->width = 1920; buffer->height = 1080; buffer->scaling = 1; std::strcpy(buffer->name, "Display fixture"); }
    *count = 1; return 0;
}
int MediaCap_GetWindows(MediaCap_Window* buffer, int* count) {
    if (!query("windows")) { *count = 0; return 0; }
    if (buffer && *count) { *buffer = {}; buffer->handle = 123; buffer->width = 640; buffer->height = 480; std::strcpy(buffer->name, "Window fixture"); }
    *count = 1; return 0;
}
int MediaCap_GetCameras(MediaCap_CameraInfo* buffer, int* count) {
    if (mode() == "slow-details") std::this_thread::sleep_for(std::chrono::seconds(30));
    if (!query("cameras")) { *count = 0; return 0; }
    if (buffer && *count) {
        *buffer = {};
        std::strcpy(buffer->camera.name, "Camera fixture");
        buffer->supported_resolutions = new MediaCap_Resolution[2]{{640, 480}, {1280, 720}};
        buffer->num_supported_resolutions = 2;
        buffer->supported_pixel_formats = new MediaCap_PixelFormat[2]{MEDIACAP_PIXEL_FORMAT_BGRA, MEDIACAP_PIXEL_FORMAT_NV12};
        buffer->num_supported_pixel_formats = 2;
    }
    *count = 1;
    if (mode() == "partial") fail(MEDIACAP_ERROR_DEVICE_UNAVAILABLE);
    return 0;
}
int MediaCap_GetCameraDevices(MediaCap_Camera* buffer, int* count) {
    if (!query("cameras")) { *count = 0; return 0; }
    if (buffer && *count) { *buffer = {}; std::strcpy(buffer->name, "Camera fixture"); }
    *count = 1;
    if (mode() == "partial") fail(MEDIACAP_ERROR_DEVICE_UNAVAILABLE);
    return 0;
}
void MediaCap_FreeCameraInfoContents(MediaCap_CameraInfo* info) {
    if (!info) return;
    delete[] info->supported_resolutions;
    delete[] info->supported_pixel_formats;
}
int MediaCap_GetAudioDevices(MediaCap_AudioDevice* buffer, int* count) {
    if (!query("audio")) { *count = 0; return 0; }
    if (buffer && *count) { *buffer = {}; buffer->type = MEDIACAP_AUDIO_DEVICE_TYPE_INPUT; std::strcpy(buffer->name, "Compatibility input"); }
    *count = 1; return 0;
}
int MediaCap_GetAudioCaptureDevices(MediaCap_AudioCaptureDevice** devices, size_t* count) {
    *devices = nullptr; *count = 0;
    if (!query("audio")) return mode() == "empty";
    *devices = new MediaCap_AudioCaptureDevice[3]{}; *count = 3;
    const char* ids[]{"input-one", "input-two", "output-one"};
    const char* names[]{"Desk microphone", "USB microphone", "Speakers"};
    for (size_t n = 0; n < 3; ++n) {
        auto& d = (*devices)[n]; d.id = owned(ids[n]); d.name = owned(names[n]);
        d.kind = n == 2 ? MEDIACAP_AUDIO_CAPTURE_SYSTEM : MEDIACAP_AUDIO_CAPTURE_MIC;
        d.sample_rate = 48000; d.channels = 2; d.is_default = n != 0;
    }
    return 1;
}
void MediaCap_FreeAudioCaptureDevices(MediaCap_AudioCaptureDevice* devices, size_t count) {
    for (size_t n = 0; n < count; ++n) { delete[] devices[n].id; delete[] devices[n].name; }
    delete[] devices;
}
int MediaCap_GetAudioProcesses(MediaCap_AudioProcess** processes, size_t* count) {
    *processes = nullptr; *count = 0;
    if (!query("processes")) return mode() == "empty";
    *processes = new MediaCap_AudioProcess[2]{}; *count = 2;
    (*processes)[0] = {101, owned("Music player"), owned("music.exe"), 1};
    (*processes)[1] = {202, owned("Idle app"), owned("idle.exe"), 0};
    return 1;
}
void MediaCap_FreeAudioProcesses(MediaCap_AudioProcess* processes, size_t count) {
    for (size_t n = 0; n < count; ++n) { delete[] processes[n].name; delete[] processes[n].executable; }
    delete[] processes;
}
MediaCap_AudioSourceConfig MediaCap_DefaultAudioSourceConfig() {
    MediaCap_AudioSourceConfig c{}; c.gain = c.mix_mic_gain = c.mix_system_gain = 1; return c;
}
int MediaCap_GetFeatureRequirements(MediaCap_Feature, MediaCap_FeatureRequirements* out) {
    if (!out) return 0;
    *out = {};
    return 1;
}
int MediaCap_GetAudioCapabilities(MediaCap_AudioCapabilities* c) {
    *c = {};
    if (!query("capabilities")) return mode() == "empty";
    c->backend_enabled = 1; c->microphone = c->system = c->process = c->mix = c->system_exclusions = MEDIACAP_AUDIO_AVAILABLE;
    return 1;
}
MediaCap_Session_Handle MediaCap_Session_Create() { return new MediaCap_Session_t; }
void MediaCap_Session_Destroy(MediaCap_Session_Handle s) { delete s; }
void MediaCap_Session_AddMonitorSource(MediaCap_Session_Handle s, const MediaCap_Monitor*, const MediaCap_ScreenCaptureConfig*) {
    s->video_source = MEDIACAP_SOURCE_TYPE_MONITOR;
}
void MediaCap_Session_AddWindowSource(MediaCap_Session_Handle s, const MediaCap_Window*, const MediaCap_WindowCaptureConfig*) {
    s->video_source = MEDIACAP_SOURCE_TYPE_WINDOW;
}
void MediaCap_Session_AddCameraSource(MediaCap_Session_Handle s, const MediaCap_Camera*, const MediaCap_CameraCaptureConfig*) {
    s->video_source = MEDIACAP_SOURCE_TYPE_CAMERA;
}
void MediaCap_Session_AddAudioSource(MediaCap_Session_Handle s, const MediaCap_AudioDevice*, const MediaCap_AudioFormat*) {
    s->audio_added = true;
}
int MediaCap_Session_AddAudioCaptureSource(MediaCap_Session_Handle s, const MediaCap_AudioSourceConfig* c) {
    const char* expected = std::getenv("MC_TEST_EXPECT_DEVICE");
    if (expected && (!c->device_id || std::strcmp(expected, c->device_id) != 0)) { fail(MEDIACAP_ERROR_INVALID_ARGUMENT); return 0; }
    s->audio_added = true;
    return 1;
}
int MediaCap_Session_SetFrameCallbacks(MediaCap_Session_Handle s, MediaCap_AudioFrameCallback audio,
                                      MediaCap_VideoFrameCallback video, void* data) {
    s->audio = audio;
    s->video = video;
    s->user_data = data;
    return 1;
}
int MediaCap_Session_StartCapture(MediaCap_Session_Handle s) {
    deliver_frames(s);
    if (mode() == "capture-error") {
        fail(MEDIACAP_ERROR_CAPTURE_FAILED, MEDIACAP_ERROR_COMPONENT_CAPTURE);
        return 0;
    }
    s->capturing = true;
    return 1;
}
void MediaCap_Session_StopCapture(MediaCap_Session_Handle s) { s->capturing = false; }
int MediaCap_Session_IsCapturing(MediaCap_Session_Handle s) { return s->capturing || s->recording; }
int MediaCap_CaptureMonitorScreenshot(const MediaCap_Monitor*, const MediaCap_ScreenshotConfig* config,
                                     MediaCap_ScreenshotCallback callback, void* data) {
    return screenshot(config, callback, data);
}
int MediaCap_CaptureWindowScreenshot(const MediaCap_Window*, const MediaCap_ScreenshotConfig* config,
                                    MediaCap_ScreenshotCallback callback, void* data) {
    return screenshot(config, callback, data);
}
int MediaCap_ValidateRecordingFormat(const MediaCap_RecordingConfig*) { return 1; }
int MediaCap_Session_StartRecording(MediaCap_Session_Handle s, const MediaCap_RecordingConfig* config) {
    if (mode() == "slow-start") std::this_thread::sleep_for(std::chrono::seconds(60));
    if (mode() == "record-frames") {
        std::ofstream(std::filesystem::u8path(config->output_filename), std::ios::binary) << "fixture media";
        deliver_frames(s);
        return 1;
    }
    query("record"); fail(MEDIACAP_ERROR_ENCODING_FAILED, MEDIACAP_ERROR_COMPONENT_ENCODER); return 0;
}
int MediaCap_Session_StartRecordingForDuration(MediaCap_Session_Handle s, const MediaCap_RecordingConfig* c, long long) { return MediaCap_Session_StartRecording(s, c); }
void MediaCap_Session_StopRecording(MediaCap_Session_Handle) {}
void MediaCap_Session_SetPaused(MediaCap_Session_Handle, int) {}
int MediaCap_Session_IsRecording(MediaCap_Session_Handle) { return 0; }
int MediaCap_Session_IsPaused(MediaCap_Session_Handle) { return 0; }
uint64_t MediaCap_Session_GetId(MediaCap_Session_Handle) { return 17; }
int MediaCap_Session_HasError(MediaCap_Session_Handle) { return last.code != MEDIACAP_ERROR_NONE; }
int MediaCap_Session_GetError(MediaCap_Session_Handle, MediaCap_Error* e) { *e = last; return 1; }
int MediaCap_GetLastError(MediaCap_Error* e) { *e = last; return 1; }
void MediaCap_ClearLastError() { last = {}; }
void MediaCap_SetErrorCallback(MediaCap_ErrorCallback, void*) {}
}
