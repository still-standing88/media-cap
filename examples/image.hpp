#pragma once

#include <MediaCap.hpp>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace example {
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<char> rgb;

    void copy_bgra(const MediaCap_VideoFrameView& frame) {
        if (frame.pixel_format != MEDIACAP_PIXEL_FORMAT_BGRA)
            throw std::runtime_error("Expected BGRA pixels");
        width = frame.width;
        height = frame.height;
        rgb.resize(size_t(width) * height * 3);
        for (uint32_t y = 0; y < height; ++y) {
            const auto row = frame.orientation == MEDIACAP_ORIENTATION_TOP_DOWN ? y : height - 1 - y;
            const auto* pixels = frame.data[0] + std::ptrdiff_t(row) * frame.linesize[0];
            for (uint32_t x = 0; x < width; ++x) {
                const auto dest = (size_t(y) * width + x) * 3;
                rgb[dest] = static_cast<char>(pixels[x * 4 + 2]);
                rgb[dest + 1] = static_cast<char>(pixels[x * 4 + 1]);
                rgb[dest + 2] = static_cast<char>(pixels[x * 4]);
            }
        }
    }

    void save_ppm(const char* path) const {
        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << width << ' ' << height << "\n255\n";
        file.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
        if (!file) throw std::runtime_error("Could not write image file");
    }
};
}
