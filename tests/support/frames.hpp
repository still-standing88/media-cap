#pragma once

#include "capture/Capturer.h"
#include <vector>

namespace fixture {
inline std::shared_ptr<MediaCap::PackedFrame> audio(int64_t timestamp, uint32_t samples = 480,
                                                   uint32_t rate = 48000, float value = .25f) {
    struct Block {
        std::vector<float> data;
        uint8_t* plane;
        MediaCap::PackedFrame frame;
    };
    auto block = std::make_shared<Block>();
    block->data.assign(samples, value);
    block->plane = reinterpret_cast<uint8_t*>(block->data.data());
    block->frame = MediaCap_AudioFrame{&block->plane, samples, {MEDIACAP_SAMPLE_FORMAT_FLT, rate, 1}, timestamp};
    return {block, &block->frame};
}

inline std::shared_ptr<MediaCap::PackedFrame> video(int64_t timestamp) {
    struct Image {
        std::vector<uint8_t> pixels;
        MediaCap::PackedFrame frame;
    };
    auto image = std::make_shared<Image>();
    image->pixels.resize(64 * 64 * 4);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            image->pixels[(y * 64 + x) * 4] = static_cast<uint8_t>(x + 10 * y);
    auto& frame = image->frame.emplace<MediaCap_VideoFrame>();
    frame.data[0] = image->pixels.data();
    frame.linesize[0] = 64 * 4;
    frame.width = frame.height = 64;
    frame.pixel_format = MEDIACAP_PIXEL_FORMAT_BGRA;
    frame.orientation = MEDIACAP_ORIENTATION_TOP_DOWN;
    frame.timestamp_ns = timestamp;
    return {image, &image->frame};
}
}
