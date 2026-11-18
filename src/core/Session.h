#pragma once
#include "MediaCap.hpp"
namespace MediaCap {
class SessionImpl;
}
namespace MediaCap::Native {
class Session {
public:
  Session();
  ~Session();
  Session(Session &&) noexcept;
  Session &operator=(Session &&) noexcept;
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  void add_source(const Monitor &, const ScreenCaptureConfig &);
  void add_source(const Window &, const WindowCaptureConfig &);
  void add_source(const Camera &, const CameraCaptureConfig &);
  void add_source(const AudioDevice &, const MediaCap_AudioFormat &);
  bool add_audio_source(const AudioSourceConfig &);
  bool set_frame_callbacks(MediaCap_AudioFrameCallback, MediaCap_VideoFrameCallback, void*);
  bool start_capture();
  void stop_capture();
  bool is_capturing() const;
  bool start_recording(const RecordingConfig &,
                       std::optional<std::chrono::milliseconds> = std::nullopt);
  void stop_recording();
  void set_paused(bool);
  bool is_recording() const;
  bool is_paused() const;
  uint64_t id() const;
  bool has_error() const;
  Error get_error() const;

private:
  std::shared_ptr<SessionImpl> m_impl;
};
}
