#pragma once

#include "capture/Capturer.h"

namespace MediaCap {
bool capture_screenshot(std::unique_ptr<Capturer> source, const MediaCap_ScreenshotConfig& config,
    MediaCap_ScreenshotCallback callback, void* user_data);
}
