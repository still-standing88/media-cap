#ifndef MEDIACAP_CAPTURER_H
#define MEDIACAP_CAPTURER_H

#include "MediaCap_types.h"
#include "utils/ErrorReporter.h"
#include <functional>
#include <memory>
#include <atomic>
#include <variant>

namespace MediaCap {

/**
 * @class Capturer
 * @brief Abstract base class for all media capture sources.
 */
class Capturer {
public:
    /**
     * @brief Callback type invoked on the capturer's background thread whenever a new frame is
     * available. The shared_ptr keeps the frame's backing buffer alive for as long as the
     * callback (or anything it forwards the frame to) retains it.
     */
    // Frames carry host-clock timestamps. Audio also supplies a block-start
    // clock observation for drift estimation; video passes -1.
    using FrameCallback = std::function<void(std::shared_ptr<PackedFrame>, int64_t)>;

    virtual ~Capturer() = default;

    void set_error_context(Errors::State* state, int index) { m_errors.configure(state, m_source_type, index); }

    /**
     * @brief Starts the capture thread and begins capturing media.
     */
    virtual void start() = 0;

    /**
     * @brief Stops the capture thread and releases resources.
     */
    virtual void stop() = 0;
    virtual void report_startup_timeout() {
        m_errors.report(MEDIACAP_ERROR_TIMEOUT, "Timed out waiting for this capture source's first frame");
    }

    /**
     * @brief Pauses or resumes the capture process.
     * @param is_paused True to pause, false to resume.
     */
    virtual void set_paused(bool is_paused) {
        m_is_paused.store(is_paused);
    }

    /**
     * @brief Checks if the capture process is currently running.
     * @return True if running, false otherwise.
     */
    virtual bool is_running() const {
        return m_is_running.load();
    }

    /**
     * @brief Checks if the capture process is paused.
     * @return True if paused, false otherwise.
     */
    virtual bool is_paused() const {
        return m_is_paused.load();
    }

    /**
     * @brief Sets the callback function to be invoked when a new frame is available.
     * @param callback The function to call.
     */
    void set_frame_callback(FrameCallback callback) {
        m_frame_callback = std::move(callback);
    }

    /**
     * @brief Gets the type of the capture source.
     * @return The source type.
     */
    MediaCap_SourceType get_source_type() const {
        return m_source_type;
    }

protected:
    Errors::Reporter m_errors{MEDIACAP_ERROR_COMPONENT_CAPTURE};
    FrameCallback m_frame_callback;                                    /**< Set via set_frame_callback(); invoked by subclasses as each frame is produced. */
    MediaCap_SourceType m_source_type = MEDIACAP_SOURCE_TYPE_UNKNOWN;  /**< Set by the subclass constructor; identifies what kind of source this is. */
    std::atomic<bool> m_is_running{false};                             /**< Set by start()/stop() in each subclass; guards the capture thread's lifecycle. */
    std::atomic<bool> m_is_paused{false};                              /**< Toggled by set_paused(); subclasses should check this before delivering frames. */
};

}

#endif
