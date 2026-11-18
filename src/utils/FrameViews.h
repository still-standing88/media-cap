#pragma once

#include "MediaCap_types.h"

namespace MediaCap {
inline MediaCap_VideoFrameView video_frame_view(const MediaCap_VideoFrame& frame) {
    return {
        {frame.data[0], frame.data[1], frame.data[2], frame.data[3]},
        {frame.linesize[0], frame.linesize[1], frame.linesize[2], frame.linesize[3]},
        frame.width, frame.height, frame.pixel_format, frame.orientation,
        frame.timestamp_ns, frame.frame_index
    };
}
}
