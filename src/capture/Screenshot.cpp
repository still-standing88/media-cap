#include "Screenshot.h"
#include "utils/FrameViews.h"
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <limits>
#include <vector>

namespace MediaCap {
bool capture_screenshot(std::unique_ptr<Capturer> source, const MediaCap_ScreenshotConfig& config,
    MediaCap_ScreenshotCallback callback, void* user_data) {
    constexpr size_t pixel_bytes = 4;
    constexpr auto max_width = std::numeric_limits<int32_t>::max() / pixel_bytes;
    const Errors::Reporter reporter(MEDIACAP_ERROR_COMPONENT_CAPTURE);
    if (!callback || (config.width == 0) != (config.height == 0) ||
        (!config.width && (config.x || config.y)) ||
        (config.output_width == 0) != (config.output_height == 0) ||
        config.output_width > max_width) {
        reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Invalid screenshot callback, crop, or output dimensions");
        return false;
    }

    Errors::State errors;
    std::mutex mutex;
    std::condition_variable ready;
    std::shared_ptr<PackedFrame> captured;
    auto capturer = std::move(source);
    capturer->set_error_context(&errors, 0);
    errors.on_failure = [&] {
        std::lock_guard<std::mutex> lock(mutex);
        ready.notify_one();
    };
    capturer->set_frame_callback([&](auto frame, int64_t) {
        const auto* video = std::get_if<MediaCap_VideoFrame>(frame.get());
        if (!video || !video->width || !video->height || !video->data[0]) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (captured) return;
        captured = std::move(frame);
        ready.notify_one();
    });
    capturer->start();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ready.wait_for(lock, std::chrono::milliseconds(config.timeout_ms ? config.timeout_ms : 10000),
            [&] { return captured || errors.failed(); });
    }
    capturer->stop();
    if (!captured) {
        if (!errors.failed()) capturer->report_startup_timeout();
        return false;
    }

    auto image = video_frame_view(std::get<MediaCap_VideoFrame>(*captured));
    const uint32_t crop_width = config.width ? config.width : image.width;
    const uint32_t crop_height = config.height ? config.height : image.height;
    if (config.x > image.width || crop_width > image.width - config.x ||
        config.y > image.height || crop_height > image.height - config.y) {
        reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Screenshot crop extends beyond the source image");
        return false;
    }
    if (image.pixel_format != MEDIACAP_PIXEL_FORMAT_BGRA || image.orientation != MEDIACAP_ORIENTATION_TOP_DOWN ||
        image.width > max_width || image.linesize[0] < static_cast<int32_t>(image.width * pixel_bytes)) {
        reporter.report(MEDIACAP_ERROR_CAPTURE_FAILED, "Screenshot source must provide top-down BGRA pixels");
        return false;
    }

    const uint32_t width = config.output_width ? config.output_width : crop_width;
    const uint32_t height = config.output_height ? config.output_height : crop_height;
    if (width == image.width && height == image.height && crop_width == width && crop_height == height) {
        callback(&image, user_data);
        return true;
    }

    std::vector<uint8_t> pixels;
    const size_t stride = width * pixel_bytes;
    if (height > pixels.max_size() / stride) {
        reporter.report(MEDIACAP_ERROR_INVALID_ARGUMENT, "Screenshot output dimensions are too large");
        return false;
    }
    pixels.resize(stride * height);
    const auto* crop = image.data[0] + size_t(config.y) * image.linesize[0] + config.x * pixel_bytes;
    for (uint32_t y = 0; y < height; ++y) {
        const auto source_y = uint64_t(y) * crop_height / height;
        const auto* row = crop + size_t(source_y) * image.linesize[0];
        auto* dest = pixels.data() + size_t(y) * stride;
        if (width == crop_width) {
            std::memcpy(dest, row, stride);
            continue;
        }
        for (uint32_t x = 0; x < width; ++x) {
            const auto source_x = uint64_t(x) * crop_width / width;
            std::memcpy(dest + x * pixel_bytes, row + source_x * pixel_bytes, pixel_bytes);
        }
    }
    image.data[0] = pixels.data();
    image.linesize[0] = static_cast<int32_t>(stride);
    image.width = width;
    image.height = height;
    callback(&image, user_data);
    return true;
}
}
