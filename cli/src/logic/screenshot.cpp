#include "../cli.hpp"
#include "output_path.hpp"
#include "../../../src/utils/Utf8.h"
#include "sources.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <fstream>
#include <limits>
#include <random>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#endif

namespace mccli {
namespace {
template<size_t N>
std::array<uint32_t, N> read_dimensions(std::string_view text, char separator, const char* message) {
    std::array<uint32_t, N> values{};
    for (size_t i = 0; i < N; ++i) {
        const auto split = text.find(separator);
        const auto part = text.substr(0, split);
        if (part.empty() || ((split == std::string_view::npos) != (i == N - 1))) throw UsageError(message);
        const auto result = std::from_chars(part.data(), part.data() + part.size(), values[i]);
        if (result.ec != std::errc{} || result.ptr != part.data() + part.size()) throw UsageError(message);
        if (split != std::string_view::npos) text.remove_prefix(split + 1);
    }
    return values;
}

bool bmp_extension(const std::string& name) {
    auto extension = std::filesystem::u8path(name).extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), MediaCap::Utf8::lower_ascii);
    return extension == ".bmp";
}

MediaCap_ScreenshotConfig make_config(const ScreenshotOptions& options) {
    MediaCap_ScreenshotConfig config{};
    if (options.timeout < 0) throw UsageError("--timeout must be a nonnegative number of milliseconds.");
    config.timeout_ms = static_cast<uint32_t>(options.timeout);
    if (!options.region.empty()) {
        const auto crop = read_dimensions<4>(options.region, ',', "Use --region X,Y,WIDTH,HEIGHT with nonnegative coordinates.");
        if (!crop[2] || !crop[3]) throw UsageError("Screenshot region width and height must be positive.");
        config.x = crop[0];
        config.y = crop[1];
        config.width = crop[2];
        config.height = crop[3];
    }
    if (!options.size.empty()) {
        const auto size = read_dimensions<2>(options.size, 'x', "Use --size WIDTHxHEIGHT with positive dimensions.");
        if (!size[0] || !size[1]) throw UsageError("Screenshot output width and height must be positive.");
        config.output_width = size[0];
        config.output_height = size[1];
    }
    return config;
}

constexpr uint64_t bmp_header_size = 54;

uint32_t bmp_row_bytes(uint32_t width, uint32_t height) {
    const uint64_t row = (uint64_t(width) * 3 + 3) & ~uint64_t(3);
    if (!width || !height || height > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
        row > (std::numeric_limits<uint32_t>::max() - bmp_header_size) / height)
        throw UsageError("Screenshot dimensions are too large for a BMP file.");
    return static_cast<uint32_t>(row);
}

void write_bmp(const std::filesystem::path& path, const MediaCap_VideoFrameView& frame) {
    const auto row_bytes = bmp_row_bytes(frame.width, frame.height);
    if (!frame.data[0] || frame.pixel_format != MEDIACAP_PIXEL_FORMAT_BGRA ||
        frame.orientation != MEDIACAP_ORIENTATION_TOP_DOWN || frame.linesize[0] <= 0 ||
        static_cast<uint64_t>(frame.linesize[0]) < uint64_t(frame.width) * 4 ||
        uint64_t(frame.linesize[0]) * (frame.height - 1) > std::numeric_limits<size_t>::max())
        throw std::runtime_error("Capture returned an invalid screenshot frame.");

    std::array<uint8_t, bmp_header_size> header{};
    const auto set_u32 = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i) header[offset + i] = static_cast<uint8_t>(value >> (i * 8));
    };
    header[0] = 'B';
    header[1] = 'M';
    set_u32(2, row_bytes * frame.height + static_cast<uint32_t>(bmp_header_size));
    set_u32(10, static_cast<uint32_t>(bmp_header_size));
    set_u32(14, 40);
    set_u32(18, frame.width);
    set_u32(22, frame.height);
    header[26] = 1;
    header[28] = 24;
    set_u32(34, row_bytes * frame.height);

    std::vector<uint8_t> row(row_bytes);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(header.data()), header.size());
    for (auto y = frame.height; file && y > 0; --y) {
        const auto* pixels = frame.data[0] + size_t(y - 1) * frame.linesize[0];
        for (size_t x = 0; x < frame.width; ++x)
            std::copy_n(pixels + x * 4, 3, row.data() + x * 3);
        file.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    file.close();
    if (!file) throw std::runtime_error("Cannot write screenshot: " + path.u8string());
}

struct ScreenshotResult {
    const std::filesystem::path& path;
    MediaCap::Resolution size{};
    std::exception_ptr failure;

    static void receive(const MediaCap_VideoFrameView* frame, void* context) noexcept {
        auto& result = *static_cast<ScreenshotResult*>(context);
        try {
            if (!frame) throw std::runtime_error("Capture returned no screenshot frame.");
            write_bmp(result.path, *frame);
            result.size = {frame->width, frame->height};
        } catch (...) { result.failure = std::current_exception(); }
    }
};

class StagedImage {
    std::filesystem::path directory_;
    std::filesystem::path path_;

public:
    explicit StagedImage(const std::filesystem::path& destination) {
        std::random_device random;
        for (int attempt = 0; attempt < 16; ++attempt) {
            auto candidate = destination.parent_path() /
                (".mediacap-image-" + std::to_string(random()) + '-' + std::to_string(random()));
            auto staged_file = candidate / "image.bmp";
#ifdef _WIN32
            if (!std::filesystem::create_directory(candidate)) continue;
#else
            if (mkdir(candidate.c_str(), S_IRWXU) != 0) {
                if (errno == EEXIST) continue;
                throw std::runtime_error("Cannot stage the screenshot in the output folder.");
            }
#endif
            directory_ = std::move(candidate);
            path_ = std::move(staged_file);
            return;
        }
        throw std::runtime_error("Cannot stage the screenshot in the output folder.");
    }

    const std::filesystem::path& path() const noexcept { return path_; }

    void commit(const std::filesystem::path& destination) const {
#ifdef _WIN32
        if (!MoveFileExW(path().c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
            throw std::runtime_error("Cannot save the screenshot: " + destination.u8string());
#else
        std::filesystem::rename(path(), destination);
#endif
    }

    ~StagedImage() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        std::filesystem::remove(directory_, ignored);
    }
};
}

void screenshot(const ScreenshotOptions& options, Output& output) {
    if (!bmp_extension(options.output)) throw UsageError("Screenshot output must use a .bmp filename.");
    const auto config = make_config(options);
    if (config.output_width) bmp_row_bytes(config.output_width, config.output_height);
    else if (config.width) bmp_row_bytes(config.width, config.height);

    OutputPath path(options.output, options.overwrite);
    StagedImage image(path.path());
    ScreenshotResult result{image.path(), {}, {}};
    bool success = false;
    if (options.kind == "screen" || options.kind == "monitor")
        success = MediaCap::capture_screenshot(select_monitor(options.monitor), ScreenshotResult::receive, &result, config);
    else
        success = MediaCap::capture_screenshot(select_window(options.window, options.window_title), ScreenshotResult::receive, &result, config);
    if (result.failure) std::rethrow_exception(result.failure);
    if (!success) check_error();
    if (!success || !result.size.width) throw std::runtime_error("Screenshot capture did not deliver an image.");
    image.commit(path.path());
    output.event("screenshot", "\"output\":" + quote(options.output) + ",\"width\":" + std::to_string(result.size.width) +
        ",\"height\":" + std::to_string(result.size.height) + ",\"format\":\"bmp\"", "Saved " + options.output + '.');
}
}
