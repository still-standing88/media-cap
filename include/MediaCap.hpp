/** @file MediaCap.hpp
 * @brief C++17 session, discovery and error interface over the C ABI.
 */
#ifndef MEDIACAP_H_API
#define MEDIACAP_H_API

#include "MediaCap_c.h"
#include "MediaCap_types.h"
#include "MediaCap_audio.h"
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <optional>
#include <future>
#include <functional>

/** @brief Public C++ value types and recording operations. */
namespace MediaCap {
    /** @brief C-compatible flexaudio source configuration. */
    using AudioSourceConfig = MediaCap_AudioSourceConfig;
    /** @brief C-compatible runtime audio capability snapshot. */
    using AudioCapabilities = MediaCap_AudioCapabilities;
    /** @brief Static prerequisites for a feature on the current build's platform. */
    inline std::optional<MediaCap_FeatureRequirements> get_feature_requirements(MediaCap_Feature feature) {
        MediaCap_FeatureRequirements result{};
        if (!MediaCap_GetFeatureRequirements(feature, &result)) return std::nullopt;
        return result;
    }
    /** @brief Audio device with owned strings; IDs belong to flexaudio discovery. */
    struct AudioCaptureDevice {
        std::string id; /**< Selectable ID. */
        std::string name; /**< Display name. */
        MediaCap_AudioCaptureKind kind; /**< MIC or SYSTEM. */
        uint32_t sample_rate; /**< Native rate, or zero when unknown. */
        uint16_t channels; /**< Native channels, or zero when unknown. */
        bool is_default; /**< Default input/output. */
    };
    /** @brief Audio process candidate with owned strings. */
    struct AudioProcess {
        uint32_t pid; /**< Process identifier. */
        std::string name; /**< Display name. */
        std::string executable; /**< Executable basename, empty if unknown. */
        int output_active; /**< -1 unknown, 0 inactive, 1 active. */
    };
    /** @brief Returns capture defaults including unity gains.
     * @return MIC routing, unity gains and backend-default rate/channels/queue.
     */
    AudioSourceConfig default_audio_source_config();
    /** @brief Runtime support, without opening capture.
     * @return Capability snapshot; failures are reported globally.
     */
    AudioCapabilities get_audio_capabilities();
    /** @brief Lists flexaudio inputs/outputs; failures use the global error reporter.
     * @return Owned device values; empty on failure or when no devices are available.
     */
    std::vector<AudioCaptureDevice> get_audio_capture_devices();
    /** @brief Lists process audio candidates; failures use the global error reporter.
     * @return Owned process values; empty on failure or when no candidates are available.
     */
    std::vector<AudioProcess> get_audio_processes();

    using Error = MediaCap_Error; /**< Copyable, structured error record. */
    using ErrorCallback = std::function<void(const Error&)>; /**< Global diagnostic listener. */
    /**
     * @brief Installs the process-wide error listener; an empty callback disables it.
     * Callbacks can run concurrently on reporting threads. Keep them short, copy/queue
     * the record for application handling, and never stop/destroy/mutate sessions from
     * inside them. Callback exceptions are contained. Replacing the listener does not
     * wait for calls already in flight; captured state must outlive those calls.
     * @param callback Listener replacing the shared C/C++ listener slot, or empty to disable.
     */
    void set_error_callback(ErrorCallback callback);
    /** @brief Returns the most recently reported global error; success does not clear it.
     * @return Error copy; code NONE means no error has been retained.
     */
    Error get_last_error();
    /** @brief Clears the global diagnostic record without clearing any session failure. */
    void clear_last_error();


    /**
     * @brief A camera together with the resolutions and pixel formats it supports.
     *
     * Unlike the C API's MediaCap_CameraInfo, this is a pure value type - `supported_resolutions`
     * and `supported_pixel_formats` are owned `std::vector`s with no manual cleanup required.
     */
    struct CameraDetail {
        MediaCap_Camera camera;                                       /**< The underlying camera identity. */
        std::vector<MediaCap_Resolution> supported_resolutions;       /**< Resolutions this camera can capture at. */
        std::vector<MediaCap_PixelFormat> supported_pixel_formats;    /**< Pixel formats this camera can deliver. */
    };

    /**
     * @brief Gets a list of all available display monitors.
     * @return A vector of Monitor objects.
     */
    std::vector<Monitor> get_monitors();

    /**
     * @brief Gets a list of all currently open, visible windows.
     * @return A vector of Window objects.
     */
    std::vector<Window> get_windows();

    /** @brief Captures a monitor screenshot and delivers borrowed BGRA pixels to the callback.
     * @param monitor The monitor to capture, as returned by get_monitors().
     * @param callback Called synchronously on success. The frame and pixels expire when it returns.
     * @param user_data Application state passed unchanged to the callback.
     * @param config Crop, output dimensions and first-frame timeout.
     * @return True if an image was delivered. On failure, inspect get_last_error().
     */
    bool capture_screenshot(const Monitor& monitor, MediaCap_ScreenshotCallback callback,
        void* user_data = nullptr, const MediaCap_ScreenshotConfig& config = {});

    /** @brief Captures a window screenshot and delivers borrowed BGRA pixels to the callback.
     * @param window The window to capture, as returned by get_windows().
     * @param callback Called synchronously on success. The frame and pixels expire when it returns.
     * @param user_data Application state passed unchanged to the callback.
     * @param config Crop, output dimensions and first-frame timeout.
     * @return True if an image was delivered. On failure, inspect get_last_error().
     */
    bool capture_screenshot(const Window& window, MediaCap_ScreenshotCallback callback,
        void* user_data = nullptr, const MediaCap_ScreenshotConfig& config = {});

    /**
     * @brief Lists camera identities without opening devices or querying formats.
     * @return Camera identities, including devices that may currently be busy.
     */
    std::vector<Camera> get_camera_devices();

    /**
     * @brief Gets a list of all available cameras along with their capabilities.
     * @return A vector of CameraDetail objects.
     */
    std::vector<CameraDetail> get_cameras();

    /**
     * @brief Gets a list of all available audio devices (input and loopback/output).
     * @return A vector of AudioDevice objects.
     */
    std::vector<AudioDevice> get_audio_devices();

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

    /**
     * @class Session
     * @brief A recording session with one video source and/or mixed audio sources in one file.
     *
     * Not copyable (only movable). Not thread-safe in general: add_source()/start_recording()/
     * stop_recording()/set_paused() must not be called concurrently with each other or with
     * themselves on the same instance, but is_recording()/is_paused() may be called concurrently
     * with any of the above from another thread. Calling stop_recording() clears the session's
     * source list, so add_source() must be called again before starting a subsequent recording.
     * No operation may race with destruction or move. Multiple video additions are not a
     * compositor or separate-track API; use one video source per session.
     */
    class Session {
    public:
        /**
         * @brief Constructs an empty session with no sources and no active recording.
         * @throws std::bad_alloc If a session handle cannot be created.
         */
        Session();

        /**
         * @brief Destroys the session, stopping any in-progress recording first.
         */
        ~Session();

        /**
         * @brief Move-constructs a session, transferring ownership of the other session's state.
         */
        Session(Session&&) noexcept;

        /**
         * @brief Move-assigns a session, transferring ownership of the other session's state.
         * @return Reference to this session.
         */
        Session& operator=(Session&&) noexcept;

        /** @brief Sessions cannot be copied; use move construction to transfer ownership. */
        Session(const Session&) = delete;
        /** @brief Sessions cannot be copy-assigned. */
        Session& operator=(const Session&) = delete;

        /**
         * @brief Adds a monitor (screen) capture source. A no-op if the session is already
         * recording.
         * @param monitor The monitor to capture, as returned by get_monitors().
         * @param config Capture parameters (poll rate) for this source.
         */
        void add_source(const Monitor& monitor, const ScreenCaptureConfig& config);

        /**
         * @brief Adds a window capture source. A no-op if the session is already recording.
         * @param window The window to capture, as returned by get_windows().
         * @param config Capture parameters (poll rate) for this source.
         */
        void add_source(const Window& window, const WindowCaptureConfig& config);

        /**
         * @brief Adds a camera capture source. A no-op if the session is already recording.
         * @param camera The camera to capture, as returned by get_cameras().
         * @param config Requested resolution/framerate/pixel format for this source.
         */
        void add_source(const Camera& camera, const CameraCaptureConfig& config);

        /**
         * @brief Adds an audio capture source. A no-op if the session is already recording.
         *
         * Audio sources require `audio_config` on RecordingConfig. Each source is converted
         * to its rate/channel layout and mixed on the session timeline. Equal source gains
         * are normalized by the number of configured audio sources to reserve headroom.
         * WASAPI loopback keeps a silent render stream open on the selected output device
         * while capturing, so idle periods still deliver continuous audio samples.
         * @param device The audio device to capture, as returned by get_audio_devices().
         * @param audio_format Sample format/rate/channel layout this device should be captured at.
         */
        void add_source(const AudioDevice& device, const MediaCap_AudioFormat& audio_format);
        /** @brief Adds microphone/system/process/mix capture through flexaudio, copying the configuration.
         * Returns false for invalid/unsupported routing or when recording. Unsupported settings are
         * rejected; selected devices are never silently replaced by broader system capture.
         * @param config Source settings; strings and exclusion arrays are copied during this call.
         * @return True when added; false on failure, with diagnostics through the error API.
         */
        bool add_audio_source(const AudioSourceConfig& config);

        /**
         * @brief Starts recording all previously-added sources to a single output file.
         *
         * `config` (including the pointees of its `video_config`/`audio_config` members and
         * `output_filename`) is only read synchronously during this call and may be freed/reused
         * immediately after it returns. If the session is already recording, this is a no-op that
         * returns true.
         *
         * This call first starts the devices and waits up to ten seconds for every source
         * to deliver a frame. `is_recording()` remains false during preparation. The output
         * file and shared recording timeline start after readiness; preparation is excluded
         * from `duration`. The latest prepared video image is held at media time zero.
         * A preparation/encoder startup failure stops and clears the sources and returns
         * false; device cleanup can take additional time beyond the readiness wait.
         * Earlier validation failures can retain unopened sources. After an ambiguous failed
         * start, create a fresh Session to avoid adding duplicate sources. A new start attempt
         * clears the retained session error unless already recording; copy it before retrying.
         * An unexpected execution gap (such as sleep) ends the recording at the last continuous
         * cutoff with DEVICE_LOST. Restart explicitly after stop; capture does not auto-resume.
         * @param config Output file path, container, and codec configuration.
         * @param duration Optional automatic-stop duration; if unset, recording continues until
         * stop_recording() is called. This is elapsed wall duration after preparation, including pauses; paused
         * intervals are omitted from the media. A duration <= 0 stops almost immediately.
         * @return True on success, false on failure (e.g. an audio source was added
         * without an `audio_config`, a video source without a `video_config`, a source failed
         * to deliver its first frame, or the muxer failed to start).
         */
        bool start_recording(const RecordingConfig& config, std::optional<std::chrono::milliseconds> duration = std::nullopt);

        /** @brief Registers raw frame listeners with the lifetime and threading rules of MediaCap_Session_SetFrameCallbacks().
         * @param audio_callback Listener for captured audio blocks, or nullptr to disable audio delivery.
         * @param video_callback Listener for captured video frames, or nullptr to disable video delivery.
         * @param user_data Application state passed unchanged to each listener. Keep it alive until the session stops.
         * @return True if registered. Returns false while capture or recording is active.
         */
        bool set_frame_callbacks(MediaCap_AudioFrameCallback audio_callback,
            MediaCap_VideoFrameCallback video_callback, void* user_data = nullptr);

        /** @brief Starts the configured sources for callback delivery without opening a recording file.
         * @return True when capture-only mode is active. On failure, inspect get_error().
         */
        bool start_capture();

        /** @brief Stops capture-only mode, waits for callbacks, and clears the source list. */
        void stop_capture();

        /** @brief Checks whether capture-only mode or recording is active. Safe to poll concurrently.
         * @return True when capture-only mode or recording is active.
         */
        bool is_capturing() const;

        /**
         * @brief Stops an in-progress recording, finalizes the output file, and clears the
         * session's source list. Blocks until capture threads have joined and the muxer has
         * finished writing. A no-op if not currently recording.
         */
        void stop_recording();

        /**
         * @brief Pauses or resumes frame delivery to the muxer without stopping capture threads.
         * Paused intervals are removed from both audio and video timestamps. A no-op if not recording.
         * @param is_paused True to pause, false to resume.
         */
        void set_paused(bool is_paused);

        /**
         * @brief Checks if the session is currently recording. Safe to call concurrently with
         * other Session methods on the same instance from another thread.
         * @return True if recording, false otherwise.
         */
        bool is_recording() const;

        /**
         * @brief Checks if a currently-recording session is paused. Safe to call concurrently
         * with other Session methods on the same instance from another thread.
         * @return True if paused, false otherwise.
         */
        bool is_paused() const;

        /** @brief Stable identifier for correlating global error callbacks with this session.
         * @return Nonzero session ID, or zero for a moved-from object without a new handle.
         */
        uint64_t id() const;
        /** @brief True after a reported recording failure, including after stop; safe to poll concurrently.
         * @return Whether this session has a retained failure.
         */
        bool has_error() const;
        /**
         * @brief First failure of the current/last recording, or code NONE. Safe to call concurrently.
         * Persists after stop; the next start attempt clears it. Check after stop to include
         * encoder flush/output errors. A failed recording may contain only partial media.
         * @return A copy of the retained error, or a zero-initialized record when none exists.
         */
        Error get_error() const;

    private:
        // Created/destroyed exclusively by the DLL's C ABI. No STL owner crosses it.
        MediaCap_Session_Handle m_handle = nullptr;
        MediaCap_Session_Handle ensure_handle();
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace MediaCap

#include "MediaCap_inline.hpp"

#endif // MEDIACAP_H_API
