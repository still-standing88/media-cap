#ifndef MEDIACAP_CAMERACAPTURER_H
#define MEDIACAP_CAMERACAPTURER_H

#include "capture/Capturer.h"
#include "third_party/CameraCapture/include/ccap.h"
#include <thread>
#include <variant>

namespace MediaCap {

class CameraCapturer : public Capturer {
public:
    /**
     * @brief Constructs a CameraCapturer.
     * @param camera The camera device to capture from.
     * @param config The desired capture configuration (resolution, fps, etc.).
     */
    CameraCapturer(const MediaCap_Camera& camera, const MediaCap_CameraCaptureConfig& config);
    ~CameraCapturer() override;

    void start() override;
    void stop() override;

private:
    void capture_thread_func();

    ccap::Provider m_provider;
    MediaCap_Camera m_camera_info;
    MediaCap_CameraCaptureConfig m_capture_config;

    std::thread m_capture_thread;
};

}

#endif
