#pragma once

#include <array>
#include <string_view>

namespace mccli {
inline constexpr std::array<std::string_view, 9> recording_containers{
    "auto", "mp4", "mkv", "webm", "m4a", "flac", "mp3", "wav", "opus"};
inline constexpr std::array<std::string_view, 4> recording_video_codecs{"auto", "h264", "vp9", "av1"};
inline constexpr std::array<std::string_view, 7> recording_audio_codecs{
    "auto", "aac", "opus", "vorbis", "flac", "mp3", "pcm"};
inline constexpr std::array<std::string_view, 6> camera_pixel_formats{
    "bgra", "bgr24", "rgba", "rgb24", "nv12", "i420"};
inline constexpr std::array<std::string_view, 4> compatibility_sample_formats{"u8", "s16", "s32", "f32"};
}
