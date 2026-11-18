#pragma once
// Client-side C++ convenience layer. Only plain C data and opaque handles cross
// the DLL boundary, including when the application and library use different
// CRTs.
#include <mutex>
#include <new>
#include <utility>

namespace MediaCap {
namespace detail {
template <class T, class F> inline std::vector<T> inventory(F enumerate) {
  int count = 0;
  if (enumerate(nullptr, &count) != 0 || count <= 0)
    return {};
  std::vector<T> out(static_cast<size_t>(count));
  if (enumerate(out.data(), &count) != 0)
    return {};
  out.resize(static_cast<size_t>(count));
  return out;
}
struct Listener {
  std::mutex mutex;
  std::shared_ptr<const ErrorCallback> callback;
};
inline Listener &listener() {
  static Listener value;
  return value;
}
inline void notify(const MediaCap_Error *error, void *) noexcept {
  try {
    std::shared_ptr<const ErrorCallback> callback;
    {
      std::lock_guard<std::mutex> lock(listener().mutex);
      callback = listener().callback;
    }
    if (callback && error)
      (*callback)(*error);
  } catch (...) {
  }
}
} // namespace detail
inline void set_error_callback(ErrorCallback callback) {
  auto next = callback
                  ? std::make_shared<const ErrorCallback>(std::move(callback))
                  : nullptr;
  const bool enabled = bool(next);
  {
    std::lock_guard<std::mutex> lock(detail::listener().mutex);
    detail::listener().callback.swap(next);
  }
  MediaCap_SetErrorCallback(enabled ? detail::notify : nullptr, nullptr);
}
inline Error get_last_error() {
  Error out{};
  MediaCap_GetLastError(&out);
  return out;
}
inline void clear_last_error() { MediaCap_ClearLastError(); }
inline AudioSourceConfig default_audio_source_config() {
  return MediaCap_DefaultAudioSourceConfig();
}
inline AudioCapabilities get_audio_capabilities() {
  AudioCapabilities out{};
  MediaCap_GetAudioCapabilities(&out);
  return out;
}
inline std::vector<Monitor> get_monitors() {
  return detail::inventory<Monitor>(MediaCap_GetMonitors);
}
inline std::vector<Window> get_windows() {
  return detail::inventory<Window>(MediaCap_GetWindows);
}
inline std::vector<Camera> get_camera_devices() {
  return detail::inventory<Camera>(MediaCap_GetCameraDevices);
}
inline std::vector<AudioDevice> get_audio_devices() {
  return detail::inventory<AudioDevice>(MediaCap_GetAudioDevices);
}
inline std::vector<CameraDetail> get_cameras() {
  int count = 0;
  if (MediaCap_GetCameras(nullptr, &count) != 0 || count <= 0)
    return {};
  struct Inventory {
    std::vector<MediaCap_CameraInfo> items;
    ~Inventory() {
      for (auto &item : items)
        MediaCap_FreeCameraInfoContents(&item);
    }
  } raw{std::vector<MediaCap_CameraInfo>(static_cast<size_t>(count))};
  if (MediaCap_GetCameras(raw.items.data(), &count) != 0)
    return {};
  std::vector<CameraDetail> out;
  out.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const auto &item = raw.items[i];
    CameraDetail camera{};
    camera.camera = item.camera;
    if (item.num_supported_resolutions)
      camera.supported_resolutions.assign(item.supported_resolutions,
                                          item.supported_resolutions +
                                              item.num_supported_resolutions);
    if (item.num_supported_pixel_formats)
      camera.supported_pixel_formats.assign(
          item.supported_pixel_formats,
          item.supported_pixel_formats + item.num_supported_pixel_formats);
    out.push_back(std::move(camera));
  }
  return out;
}
inline std::vector<AudioCaptureDevice> get_audio_capture_devices() {
  struct Inventory {
    MediaCap_AudioCaptureDevice *items = nullptr;
    size_t count = 0;
    ~Inventory() { MediaCap_FreeAudioCaptureDevices(items, count); }
  } raw;
  if (!MediaCap_GetAudioCaptureDevices(&raw.items, &raw.count))
    return {};
  std::vector<AudioCaptureDevice> out;
  out.reserve(raw.count);
  for (size_t i = 0; i < raw.count; ++i) {
    const auto &item = raw.items[i];
    out.push_back({item.id ? item.id : "", item.name ? item.name : "",
                   item.kind, item.sample_rate, item.channels,
                   item.is_default != 0});
  }
  return out;
}
inline std::vector<AudioProcess> get_audio_processes() {
  struct Inventory {
    MediaCap_AudioProcess *items = nullptr;
    size_t count = 0;
    ~Inventory() { MediaCap_FreeAudioProcesses(items, count); }
  } raw;
  if (!MediaCap_GetAudioProcesses(&raw.items, &raw.count))
    return {};
  std::vector<AudioProcess> out;
  out.reserve(raw.count);
  for (size_t i = 0; i < raw.count; ++i) {
    const auto &item = raw.items[i];
    out.push_back({item.pid, item.name ? item.name : "",
                   item.executable ? item.executable : "", item.output_active});
  }
  return out;
}
inline MediaCap_Session_Handle Session::ensure_handle() {
  if (!m_handle)
    m_handle = MediaCap_Session_Create();
  if (!m_handle)
    throw std::bad_alloc();
  return m_handle;
}
inline Session::Session() { ensure_handle(); }
inline Session::~Session() { MediaCap_Session_Destroy(m_handle); }
inline Session::Session(Session &&other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr)) {}
inline Session &Session::operator=(Session &&other) noexcept {
  if (this != &other) {
    MediaCap_Session_Destroy(m_handle);
    m_handle = std::exchange(other.m_handle, nullptr);
  }
  return *this;
}
inline void Session::add_source(const Monitor &value,
                                const ScreenCaptureConfig &config) {
  MediaCap_Session_AddMonitorSource(ensure_handle(), &value, &config);
}
inline void Session::add_source(const Window &value,
                                const WindowCaptureConfig &config) {
  MediaCap_Session_AddWindowSource(ensure_handle(), &value, &config);
}
inline void Session::add_source(const Camera &value,
                                const CameraCaptureConfig &config) {
  MediaCap_Session_AddCameraSource(ensure_handle(), &value, &config);
}
inline void Session::add_source(const AudioDevice &value,
                                const MediaCap_AudioFormat &config) {
  MediaCap_Session_AddAudioSource(ensure_handle(), &value, &config);
}
inline bool Session::add_audio_source(const AudioSourceConfig &config) {
  return MediaCap_Session_AddAudioCaptureSource(ensure_handle(), &config) != 0;
}
inline bool capture_screenshot(const Monitor& monitor, MediaCap_ScreenshotCallback callback,
                               void* user_data, const MediaCap_ScreenshotConfig& config) {
  return MediaCap_CaptureMonitorScreenshot(&monitor, &config, callback, user_data) != 0;
}
inline bool capture_screenshot(const Window& window, MediaCap_ScreenshotCallback callback,
                               void* user_data, const MediaCap_ScreenshotConfig& config) {
  return MediaCap_CaptureWindowScreenshot(&window, &config, callback, user_data) != 0;
}
inline bool Session::set_frame_callbacks(MediaCap_AudioFrameCallback audio_callback,
                                        MediaCap_VideoFrameCallback video_callback, void* user_data) {
  return MediaCap_Session_SetFrameCallbacks(ensure_handle(), audio_callback, video_callback, user_data) != 0;
}
inline bool Session::start_capture() { return MediaCap_Session_StartCapture(m_handle) != 0; }
inline void Session::stop_capture() { MediaCap_Session_StopCapture(m_handle); }
inline bool Session::is_capturing() const { return MediaCap_Session_IsCapturing(m_handle) != 0; }
inline bool
Session::start_recording(const RecordingConfig &config,
                         std::optional<std::chrono::milliseconds> duration) {
  return duration ? MediaCap_Session_StartRecordingForDuration(
                        m_handle, &config, duration->count()) != 0
                  : MediaCap_Session_StartRecording(m_handle, &config) != 0;
}
inline void Session::stop_recording() {
  MediaCap_Session_StopRecording(m_handle);
}
inline void Session::set_paused(bool value) {
  MediaCap_Session_SetPaused(m_handle, value);
}
inline bool Session::is_recording() const {
  return MediaCap_Session_IsRecording(m_handle) != 0;
}
inline bool Session::is_paused() const {
  return MediaCap_Session_IsPaused(m_handle) != 0;
}
inline uint64_t Session::id() const { return MediaCap_Session_GetId(m_handle); }
inline bool Session::has_error() const {
  return MediaCap_Session_HasError(m_handle) != 0;
}
inline Error Session::get_error() const {
  Error out{};
  MediaCap_Session_GetError(m_handle, &out);
  return out;
}
} // namespace MediaCap
