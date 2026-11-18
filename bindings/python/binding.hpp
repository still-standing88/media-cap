#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "MediaCap_c.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace mediacap::python {
namespace py = pybind11;

inline py::str decode(const char* data, size_t length, const char* errors = "replace") {
    auto* value = PyUnicode_DecodeUTF8(data, static_cast<Py_ssize_t>(length), errors);
    if (!value) throw py::error_already_set();
    return py::reinterpret_steal<py::str>(value);
}

template <size_t N>
py::str display_text(const char (&value)[N]) {
    return decode(value, static_cast<size_t>(std::find(value, value + N, '\0') - value));
}

inline std::string checked_text(const py::str& text) {
    Py_ssize_t size = 0;
    const auto* bytes = PyUnicode_AsUTF8AndSize(text.ptr(), &size);
    if (!bytes) throw py::error_already_set();
    std::string result(bytes, static_cast<size_t>(size));
    if (result.find('\0') != std::string::npos) throw py::value_error("Embedded NUL");
    return result;
}

struct AudioSourceConfig {
    MediaCap_AudioSourceConfig settings = MediaCap_DefaultAudioSourceConfig();
    std::optional<std::string> device_id;
    std::optional<std::string> mix_mic_device_id;
    std::optional<std::string> mix_system_device_id;
    std::vector<uint32_t> exclude_pids;
};

struct RecordingConfig {
    py::object output_filename = py::str("");
    MediaCap_ContainerFormat container = MEDIACAP_CONTAINER_DEFAULT;
    std::optional<MediaCap_VideoEncoderConfig> video_config;
    std::optional<MediaCap_AudioEncoderConfig> audio_config;
};

struct CameraDetail {
    MediaCap_Camera camera;
    std::vector<MediaCap_Resolution> supported_resolutions;
    std::vector<MediaCap_PixelFormat> supported_pixel_formats;
};

struct AudioCaptureDevice {
    py::str id, name;
    MediaCap_AudioCaptureKind kind;
    uint32_t sample_rate;
    uint16_t channels;
    bool is_default;
};

struct AudioProcess {
    uint32_t pid;
    py::str name;
    py::object executable;
    int output_active;
};

py::object copy_video_frame(const MediaCap_VideoFrameView& frame);
py::object copy_audio_frame(const MediaCap_AudioFrameView& frame);
void bind_types(py::module_& module);
void bind_enums(py::module_& module);
void bind_discovery(py::module_& module);
void bind_frames(py::module_& module);
void bind_session(py::module_& module);
}
