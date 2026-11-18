#include "binding.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>

namespace mediacap::python {
namespace {
struct VideoFrame {
    py::tuple data;
    std::array<int64_t, 4> linesize{};
    uint32_t width, height;
    MediaCap_PixelFormat pixel_format;
    MediaCap_FrameOrientation orientation;
    int64_t timestamp_ns;
    uint64_t frame_index;
};

struct AudioFrame {
    py::bytes data;
    uint32_t num_samples;
    MediaCap_AudioFormat format;
    int64_t timestamp_ns;
};

size_t checked_size(uint64_t count, uint64_t item_size) {
    constexpr auto limit = static_cast<uint64_t>((std::numeric_limits<Py_ssize_t>::max)());
    if (item_size && count > limit / item_size) throw std::overflow_error("Frame buffer size overflow");
    return static_cast<size_t>(count * item_size);
}

struct Plane {
    uint64_t width = 0, rows = 0;
};

std::array<Plane, 4> planes(const MediaCap_VideoFrameView& frame) {
    const uint64_t width = frame.width, height = frame.height;
    const Plane chroma{(width + 1) / 2, (height + 1) / 2};
    switch (frame.pixel_format) {
    case MEDIACAP_PIXEL_FORMAT_BGRA:
    case MEDIACAP_PIXEL_FORMAT_RGBA: return {{{width * 4, height}}};
    case MEDIACAP_PIXEL_FORMAT_BGR24:
    case MEDIACAP_PIXEL_FORMAT_RGB24: return {{{width * 3, height}}};
    case MEDIACAP_PIXEL_FORMAT_NV12: return {{{width, height}, {chroma.width * 2, chroma.rows}}};
    case MEDIACAP_PIXEL_FORMAT_I420: return {{{width, height}, chroma, chroma}};
    default: throw std::runtime_error("Unsupported video frame layout");
    }
}

py::bytes copy_plane(const uint8_t* data, int32_t stride, const Plane& plane, int64_t& output_stride) {
    const int64_t offset = stride;
    const auto distance = static_cast<uint64_t>(offset < 0 ? -offset : offset);
    if (!data || !plane.width || !plane.rows || distance < plane.width)
        throw std::runtime_error("Invalid video plane");

    const auto total = checked_size(plane.rows, plane.width);
    checked_size(plane.rows - 1, distance);
    auto* bytes = PyBytes_FromStringAndSize(nullptr, static_cast<Py_ssize_t>(total));
    if (!bytes) throw py::error_already_set();
    auto result = py::reinterpret_steal<py::bytes>(bytes);
    auto* destination = PyBytes_AS_STRING(bytes);
    for (uint64_t row = 0; row < plane.rows; ++row) {
        std::memcpy(destination, data + static_cast<std::ptrdiff_t>(row) * offset,
                    static_cast<size_t>(plane.width));
        destination += plane.width;
    }
    output_stride = static_cast<int64_t>(plane.width);
    return result;
}

size_t sample_width(MediaCap_SampleFormat format) {
    switch (format) {
    case MEDIACAP_SAMPLE_FORMAT_U8: return 1;
    case MEDIACAP_SAMPLE_FORMAT_S16: return 2;
    case MEDIACAP_SAMPLE_FORMAT_S32:
    case MEDIACAP_SAMPLE_FORMAT_FLT: return 4;
    case MEDIACAP_SAMPLE_FORMAT_DBL: return 8;
    default: throw std::runtime_error("Unsupported interleaved audio frame layout");
    }
}

struct ScreenshotResult {
    py::object frame;
    std::exception_ptr error;
};

void screenshot_frame(const MediaCap_VideoFrameView* frame, void* user_data) noexcept {
    auto& result = *static_cast<ScreenshotResult*>(user_data);
    py::gil_scoped_acquire acquire;  // The screenshot callback is synchronous on this thread.
    try { result.frame = copy_video_frame(*frame); }
    catch (...) { result.error = std::current_exception(); }
}

template <class Source, class F>
py::object screenshot(F capture, Source source, MediaCap_ScreenshotConfig config) {
    ScreenshotResult result{py::none(), {}};
    bool success;
    {
        py::gil_scoped_release release;
        success = capture(&source, &config, screenshot_frame, &result) != 0;
    }
    if (result.error) std::rethrow_exception(result.error);
    return success ? result.frame : py::none();
}
}

py::object copy_video_frame(const MediaCap_VideoFrameView& frame) {
    VideoFrame output{py::tuple(4), {}, frame.width, frame.height, frame.pixel_format,
        frame.orientation, frame.timestamp_ns, frame.frame_index};
    const auto layout = planes(frame);
    for (size_t index = 0; index < layout.size(); ++index) {
        if (layout[index].rows)
            output.data[index] = copy_plane(frame.data[index], frame.linesize[index], layout[index], output.linesize[index]);
        else output.data[index] = py::none();
    }
    return py::cast(std::move(output));
}

py::object copy_audio_frame(const MediaCap_AudioFrameView& frame) {
    if (!frame.data || !frame.format.channels || !frame.format.sample_rate)
        throw std::runtime_error("Invalid audio frame");
    const auto size = checked_size(frame.num_samples, frame.format.channels * sample_width(frame.format.format));
    return py::cast(AudioFrame{py::bytes(reinterpret_cast<const char*>(frame.data), size),
        frame.num_samples, frame.format, frame.timestamp_ns});
}

void bind_frames(py::module_& module) {
    py::class_<VideoFrame>(module, "VideoFrame")
        .def_readonly("data", &VideoFrame::data)
        .def_readonly("linesize", &VideoFrame::linesize)
        .def_readonly("width", &VideoFrame::width)
        .def_readonly("height", &VideoFrame::height)
        .def_readonly("pixel_format", &VideoFrame::pixel_format)
        .def_readonly("orientation", &VideoFrame::orientation)
        .def_readonly("timestamp_ns", &VideoFrame::timestamp_ns)
        .def_readonly("frame_index", &VideoFrame::frame_index);
    py::class_<AudioFrame>(module, "AudioFrame")
        .def_readonly("data", &AudioFrame::data)
        .def_readonly("num_samples", &AudioFrame::num_samples)
        .def_readonly("format", &AudioFrame::format)
        .def_readonly("timestamp_ns", &AudioFrame::timestamp_ns);
    module.def("capture_screenshot", [](const MediaCap_Monitor& monitor, const MediaCap_ScreenshotConfig& config) {
        return screenshot(MediaCap_CaptureMonitorScreenshot, monitor, config);
    }, py::arg("source"), py::arg("config") = MediaCap_ScreenshotConfig{});
    module.def("capture_screenshot", [](const MediaCap_Window& window, const MediaCap_ScreenshotConfig& config) {
        return screenshot(MediaCap_CaptureWindowScreenshot, window, config);
    }, py::arg("source"), py::arg("config") = MediaCap_ScreenshotConfig{});
}
}
