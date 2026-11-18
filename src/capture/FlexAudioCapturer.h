#pragma once
#include "MediaCap_audio.h"
#include "capture/Capturer.h"
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace MediaCap {
struct OwnedAudioSourceConfig {
    MediaCap_AudioSourceConfig value;
    std::optional<std::string> device, mic, system;
    std::vector<uint32_t> excluded;
    explicit OwnedAudioSourceConfig(const MediaCap_AudioSourceConfig &config);
    OwnedAudioSourceConfig(const OwnedAudioSourceConfig &) = delete;
    OwnedAudioSourceConfig &operator=(const OwnedAudioSourceConfig &) = delete;
};
class FlexAudioCapturer : public Capturer {
  public:
    explicit FlexAudioCapturer(const MediaCap_AudioSourceConfig &config);
    ~FlexAudioCapturer() override { stop(); }
    void start() override;
    void stop() override;
    static bool validate(const MediaCap_AudioSourceConfig &config, const Errors::Reporter &errors);

  private:
    void run();
    OwnedAudioSourceConfig m_config;
    std::thread m_thread;
};
}
