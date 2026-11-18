#include "core/Session.h"
#include "capture/ScreenCapturer.h"
#include "capture/CameraCapturer.h"
#include "capture/AudioCapturer.h"
#include "capture/FlexAudioCapturer.h"
#include "core/SessionImpl.h"
#include <mutex>

namespace MediaCap::Native {
Session::Session() : m_impl(std::make_shared<SessionImpl>()) {}
Session::~Session() { if (m_impl) m_impl->shutdown(); }
Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        if (m_impl) m_impl->shutdown();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}
void Session::add_source(const Monitor& monitor, const ScreenCaptureConfig& config) try {
    if (!m_impl) m_impl = std::make_shared<SessionImpl>();
    if (!m_impl->is_capturing()) m_impl->capturers.push_back(std::make_unique<ScreenCapturer>(monitor, config));
} catch (...) {
    if (m_impl) m_impl->reporter.exception("Could not add capture source");
    else Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not create capture session");
}
void Session::add_source(const Window& window, const WindowCaptureConfig& config) try {
    if (!m_impl) m_impl = std::make_shared<SessionImpl>();
    if (!m_impl->is_capturing()) m_impl->capturers.push_back(std::make_unique<ScreenCapturer>(window, config));
} catch (...) {
    if (m_impl) m_impl->reporter.exception("Could not add capture source");
    else Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not create capture session");
}
void Session::add_source(const Camera& camera, const CameraCaptureConfig& config) try {
    if (!m_impl) m_impl = std::make_shared<SessionImpl>();
    if (!m_impl->is_capturing()) m_impl->capturers.push_back(std::make_unique<CameraCapturer>(camera, config));
} catch (...) {
    if (m_impl) m_impl->reporter.exception("Could not add capture source");
    else Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not create capture session");
}
void Session::add_source(const AudioDevice& device, const MediaCap_AudioFormat& format) try {
    if (!m_impl) m_impl = std::make_shared<SessionImpl>();
    if (!m_impl->is_capturing()) m_impl->capturers.push_back(std::make_unique<AudioCapturer>(device, format));
} catch (...) {
    if (m_impl) m_impl->reporter.exception("Could not add capture source");
    else Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not create capture session");
}
bool Session::start_recording(const RecordingConfig& config, std::optional<std::chrono::milliseconds> duration) {
    return m_impl && m_impl->start_recording(config, duration);
}
bool Session::add_audio_source(const AudioSourceConfig& config) try {
    if (!m_impl) m_impl = std::make_shared<SessionImpl>();
    if (m_impl->is_capturing()) {
        Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).report(MEDIACAP_ERROR_INVALID_ARGUMENT,
            "Add audio sources before starting capture");
        return false;
    }
    if (!FlexAudioCapturer::validate(config,m_impl->reporter)) return false;
    m_impl->capturers.push_back(std::make_unique<FlexAudioCapturer>(config)); return true;
} catch (...) {
    if(m_impl)m_impl->reporter.exception("Could not add flexaudio source");
    else Errors::Reporter(MEDIACAP_ERROR_COMPONENT_API).exception("Could not create audio capture session");
    return false;
}
bool Session::set_frame_callbacks(MediaCap_AudioFrameCallback audio, MediaCap_VideoFrameCallback video, void* user_data) {
    return m_impl && m_impl->set_frame_callbacks(audio, video, user_data);
}
bool Session::start_capture() { return m_impl && m_impl->start_capture(); }
void Session::stop_capture() { if (is_capturing() && !is_recording()) m_impl->ensure_stopped(); }
bool Session::is_capturing() const { return m_impl && m_impl->is_capturing(); }
void Session::stop_recording() { if (is_recording()) m_impl->ensure_stopped(); }
void Session::set_paused(bool paused) {
    if (!m_impl) return;
    std::lock_guard<std::mutex> lock(m_impl->stop_mutex);
    if (!m_impl->recording) return;
    m_impl->timeline.set_paused(paused);
    m_impl->paused.store(paused);
}
bool Session::is_recording() const { return m_impl && m_impl->recording.load(); }
bool Session::is_paused() const { return m_impl && m_impl->paused.load(); }
uint64_t Session::id() const { return m_impl ? m_impl->errors.id : 0; }
bool Session::has_error() const { return m_impl && m_impl->errors.failed(); }
Error Session::get_error() const { return m_impl ? m_impl->errors.get() : Error{}; }
}
