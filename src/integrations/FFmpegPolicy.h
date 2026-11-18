#pragma once
#include <cstring>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace MediaCap::FFmpegPolicy {
inline bool allowed(const char *configuration, const char *license) {
  return configuration && license && std::strstr(license, "LGPL") &&
         !std::strstr(configuration, "--enable-gpl") &&
         !std::strstr(configuration, "--enable-nonfree") &&
         !std::strstr(configuration, "--enable-libx264") &&
         !std::strstr(configuration, "--enable-libx265") &&
         !std::strstr(configuration, "--enable-libfdk-aac");
}
inline bool compatible() {
  return allowed(avcodec_configuration(), avcodec_license()) &&
         allowed(avformat_configuration(), avformat_license()) &&
         allowed(avutil_configuration(), avutil_license()) &&
         allowed(swresample_configuration(), swresample_license()) &&
         allowed(swscale_configuration(), swscale_license());
}
inline constexpr const char *encoders[] = {
    "libopenh264", "libvpx-vp9", "libaom-av1", "aac",      "libopus",
    "libvorbis",   "flac",       "libmp3lame", "pcm_s16le"};
}
