#pragma once

#include <MediaCap_types.h>

#include <memory>

namespace mccli {
class OutputPath;

struct FrameCounts {
    uint64_t video = 0;
    uint64_t audio = 0;
    uint64_t samples = 0;
};

class FrameSinks {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    FrameSinks(const OutputPath* video_path, const OutputPath* audio_path);
    ~FrameSinks();
    FrameSinks(const FrameSinks&) = delete;
    FrameSinks& operator=(const FrameSinks&) = delete;

    static void video_callback(size_t index, MediaCap_SourceType source, const MediaCap_VideoFrameView* view, void* data) noexcept;
    static void audio_callback(size_t index, MediaCap_SourceType source, const MediaCap_AudioFrameView* view, void* data) noexcept;
    FrameCounts counts() const;
    bool failed() const;
    void begin_writing();
    void finish();
};
}
