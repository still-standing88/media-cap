#include "binding.hpp"

namespace mediacap::python {
void bind_types(py::module_& module) {
    py::class_<MediaCap_Error>(module, "Error")
        .def_readonly("code", &MediaCap_Error::code)
        .def_readonly("component", &MediaCap_Error::component)
        .def_readonly("source_type", &MediaCap_Error::source_type)
        .def_readonly("source_index", &MediaCap_Error::source_index)
        .def_readonly("session_id", &MediaCap_Error::session_id)
        .def_readonly("native_code", &MediaCap_Error::native_code)
        .def_property_readonly("message", [](const MediaCap_Error& value) {
            return display_text(value.message);
        });

    py::class_<MediaCap_Resolution>(module, "Resolution")
        .def(py::init<>())
        .def_readwrite("width", &MediaCap_Resolution::width)
        .def_readwrite("height", &MediaCap_Resolution::height);

    py::class_<MediaCap_Monitor>(module, "Monitor")
        .def_readonly("id", &MediaCap_Monitor::id)
        .def_readonly("index", &MediaCap_Monitor::index)
        .def_readonly("offsetX", &MediaCap_Monitor::offsetX)
        .def_readonly("offsetY", &MediaCap_Monitor::offsetY)
        .def_readonly("width", &MediaCap_Monitor::width)
        .def_readonly("height", &MediaCap_Monitor::height)
        .def_readonly("scaling", &MediaCap_Monitor::scaling)
        .def_property_readonly("name", [](const MediaCap_Monitor& value) {
            return display_text(value.name);
        });

    py::class_<MediaCap_Window>(module, "Window")
        .def_readonly("handle", &MediaCap_Window::handle)
        .def_readonly("offsetX", &MediaCap_Window::offsetX)
        .def_readonly("offsetY", &MediaCap_Window::offsetY)
        .def_readonly("width", &MediaCap_Window::width)
        .def_readonly("height", &MediaCap_Window::height)
        .def_property_readonly("name", [](const MediaCap_Window& value) {
            return display_text(value.name);
        });

    py::class_<MediaCap_Camera>(module, "Camera")
        .def_readonly("index", &MediaCap_Camera::index)
        .def_property_readonly("name", [](const MediaCap_Camera& value) {
            return display_text(value.name);
        })
        .def_property_readonly("unique_id", [](const MediaCap_Camera& value) {
            return display_text(value.unique_id);
        });

    py::class_<MediaCap_CameraCaptureConfig>(module, "CameraCaptureConfig")
        .def(py::init<>())
        .def_readwrite("width", &MediaCap_CameraCaptureConfig::width)
        .def_readwrite("height", &MediaCap_CameraCaptureConfig::height)
        .def_readwrite("framerate", &MediaCap_CameraCaptureConfig::framerate)
        .def_readwrite("pixel_format", &MediaCap_CameraCaptureConfig::pixel_format);

    py::class_<MediaCap_ScreenCaptureConfig>(module, "ScreenCaptureConfig")
        .def(py::init<>())
        .def_readwrite("capture_rate_ms", &MediaCap_ScreenCaptureConfig::capture_rate_ms);

    py::class_<MediaCap_WindowCaptureConfig>(module, "WindowCaptureConfig")
        .def(py::init<>())
        .def_readwrite("capture_rate_ms", &MediaCap_WindowCaptureConfig::capture_rate_ms);

    py::class_<MediaCap_AudioDevice>(module, "AudioDevice")
        .def_property_readonly("id", [](const MediaCap_AudioDevice& value) {
            return py::bytes(reinterpret_cast<const char*>(value.id), sizeof(value.id));
        })
        .def_property_readonly("name", [](const MediaCap_AudioDevice& value) {
            return display_text(value.name);
        })
        .def_readonly("type", &MediaCap_AudioDevice::type)
        .def_readonly("is_default", &MediaCap_AudioDevice::is_default);

    py::class_<MediaCap_AudioFormat>(module, "AudioFormat")
        .def(py::init<>())
        .def_readwrite("format", &MediaCap_AudioFormat::format)
        .def_readwrite("sample_rate", &MediaCap_AudioFormat::sample_rate)
        .def_readwrite("channels", &MediaCap_AudioFormat::channels);

    py::class_<MediaCap_ScreenshotConfig>(module, "ScreenshotConfig")
        .def(py::init<>())
        .def_readwrite("x", &MediaCap_ScreenshotConfig::x)
        .def_readwrite("y", &MediaCap_ScreenshotConfig::y)
        .def_readwrite("width", &MediaCap_ScreenshotConfig::width)
        .def_readwrite("height", &MediaCap_ScreenshotConfig::height)
        .def_readwrite("output_width", &MediaCap_ScreenshotConfig::output_width)
        .def_readwrite("output_height", &MediaCap_ScreenshotConfig::output_height)
        .def_readwrite("timeout_ms", &MediaCap_ScreenshotConfig::timeout_ms);

    py::class_<MediaCap_VideoEncoderConfig>(module, "VideoEncoderConfig")
        .def(py::init<>())
        .def_readwrite("width", &MediaCap_VideoEncoderConfig::width)
        .def_readwrite("height", &MediaCap_VideoEncoderConfig::height)
        .def_readwrite("framerate", &MediaCap_VideoEncoderConfig::framerate)
        .def_readwrite("bitrate_bps", &MediaCap_VideoEncoderConfig::bitrate_bps)
        .def_readwrite("codec", &MediaCap_VideoEncoderConfig::codec);

    py::class_<MediaCap_AudioEncoderConfig>(module, "AudioEncoderConfig")
        .def(py::init<>())
        .def_readwrite("sample_rate", &MediaCap_AudioEncoderConfig::sample_rate)
        .def_readwrite("channels", &MediaCap_AudioEncoderConfig::channels)
        .def_readwrite("bitrate_bps", &MediaCap_AudioEncoderConfig::bitrate_bps)
        .def_readwrite("codec", &MediaCap_AudioEncoderConfig::codec);

    py::class_<MediaCap_AudioCapabilities>(module, "AudioCapabilities")
        .def_readonly("backend_enabled", &MediaCap_AudioCapabilities::backend_enabled)
        .def_readonly("microphone", &MediaCap_AudioCapabilities::microphone)
        .def_readonly("system", &MediaCap_AudioCapabilities::system)
        .def_readonly("process", &MediaCap_AudioCapabilities::process)
        .def_readonly("mix", &MediaCap_AudioCapabilities::mix)
        .def_readonly("system_exclusions", &MediaCap_AudioCapabilities::system_exclusions)
        .def_readonly("process_scope", &MediaCap_AudioCapabilities::process_scope)
        .def_readonly("exclusion_scope", &MediaCap_AudioCapabilities::exclusion_scope)
        .def_readonly("exclusion_honors_device", &MediaCap_AudioCapabilities::exclusion_honors_device)
        .def_readonly("max_excluded_pids", &MediaCap_AudioCapabilities::max_excluded_pids)
        .def_property_readonly("process_detail", [](const MediaCap_AudioCapabilities& value) {
            return display_text(value.process_detail);
        })
        .def_property_readonly("system_detail", [](const MediaCap_AudioCapabilities& value) {
            return display_text(value.system_detail);
        });

    py::class_<CameraDetail>(module, "CameraDetail")
        .def_readonly("camera", &CameraDetail::camera)
        .def_readonly("supported_resolutions", &CameraDetail::supported_resolutions)
        .def_readonly("supported_pixel_formats", &CameraDetail::supported_pixel_formats);
    py::class_<AudioCaptureDevice>(module, "AudioCaptureDevice")
        .def_readonly("id", &AudioCaptureDevice::id)
        .def_readonly("name", &AudioCaptureDevice::name)
        .def_readonly("kind", &AudioCaptureDevice::kind)
        .def_readonly("sample_rate", &AudioCaptureDevice::sample_rate)
        .def_readonly("channels", &AudioCaptureDevice::channels)
        .def_readonly("is_default", &AudioCaptureDevice::is_default);
    py::class_<AudioProcess>(module, "AudioProcess")
        .def_readonly("pid", &AudioProcess::pid)
        .def_readonly("name", &AudioProcess::name)
        .def_readonly("executable", &AudioProcess::executable)
        .def_readonly("output_active", &AudioProcess::output_active);

    py::class_<RecordingConfig>(module, "RecordingConfig")
        .def(py::init<>())
        .def_readwrite("output_filename", &RecordingConfig::output_filename)
        .def_readwrite("container", &RecordingConfig::container)
        .def_readwrite("video_config", &RecordingConfig::video_config)
        .def_readwrite("audio_config", &RecordingConfig::audio_config);

    auto audio = py::class_<AudioSourceConfig>(module, "AudioSourceConfig");
    audio.def(py::init<>())
        .def_readwrite("exclude_pids", &AudioSourceConfig::exclude_pids);
    const auto bind_audio_id = [&audio](const char* name, std::optional<std::string> AudioSourceConfig::* field) {
        audio.def_property(name, [field](const AudioSourceConfig& config) {
            return config.*field;
        }, [field](AudioSourceConfig& config, const std::optional<py::str>& value) {
            config.*field = value ? std::make_optional(checked_text(*value)) : std::nullopt;
        });
    };
    bind_audio_id("device_id", &AudioSourceConfig::device_id);
    bind_audio_id("mix_mic_device_id", &AudioSourceConfig::mix_mic_device_id);
    bind_audio_id("mix_system_device_id", &AudioSourceConfig::mix_system_device_id);
    const auto bind_audio_setting = [&audio](const char* name, auto field) {
        using Value = std::decay_t<decltype(AudioSourceConfig{}.settings.*field)>;
        audio.def_property(name, [field](const AudioSourceConfig& config) { return config.settings.*field; },
            [field](AudioSourceConfig& config, Value value) { config.settings.*field = value; });
    };
    bind_audio_setting("kind", &MediaCap_AudioSourceConfig::kind);
    bind_audio_setting("process_id", &MediaCap_AudioSourceConfig::process_id);
    bind_audio_setting("process_mode", &MediaCap_AudioSourceConfig::process_mode);
    bind_audio_setting("exclude_self", &MediaCap_AudioSourceConfig::exclude_self);
    bind_audio_setting("gain", &MediaCap_AudioSourceConfig::gain);
    bind_audio_setting("mix_mic_gain", &MediaCap_AudioSourceConfig::mix_mic_gain);
    bind_audio_setting("mix_system_gain", &MediaCap_AudioSourceConfig::mix_system_gain);
    bind_audio_setting("sample_rate", &MediaCap_AudioSourceConfig::sample_rate);
    bind_audio_setting("channels", &MediaCap_AudioSourceConfig::channels);
    bind_audio_setting("ring_capacity_chunks", &MediaCap_AudioSourceConfig::ring_capacity_chunks);
    module.def("default_audio_source_config", [] { return AudioSourceConfig{}; });
}
}
