/** @file MediaCap_c.h
 * @brief C session lifecycle, classic device discovery and error-reporting API.
 */
#ifndef MEDIACAP_C_API_H
#define MEDIACAP_C_API_H

#include "MediaCap_types.h"
#include "MediaCap_audio.h"
#include "MediaCap_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Host platform selected by this MediaCap build. */
typedef enum {
    MEDIACAP_PLATFORM_UNKNOWN = 0,
    MEDIACAP_PLATFORM_WINDOWS,
    MEDIACAP_PLATFORM_MACOS,
    MEDIACAP_PLATFORM_LINUX
} MediaCap_Platform;

/** @brief Capture features with platform prerequisites. Audio routes below use FlexAudio. */
typedef enum {
    MEDIACAP_FEATURE_SCREEN = 0,
    MEDIACAP_FEATURE_WINDOW,
    MEDIACAP_FEATURE_CAMERA,
    MEDIACAP_FEATURE_MICROPHONE,
    MEDIACAP_FEATURE_SYSTEM_AUDIO,
    MEDIACAP_FEATURE_PROCESS_AUDIO,
    MEDIACAP_FEATURE_MIX_AUDIO,
    MEDIACAP_FEATURE_AUDIO_EXCLUSION,
    MEDIACAP_FEATURE_CLASSIC_AUDIO
} MediaCap_Feature;

/** @brief Feature prerequisites. These bits describe requirements, rather than probe results. */
typedef enum {
    MEDIACAP_REQUIRES_DISPLAY_SESSION = 1u << 0,
    MEDIACAP_REQUIRES_X11 = 1u << 1,
    MEDIACAP_REQUIRES_CAMERA_DEVICE = 1u << 2,
    MEDIACAP_REQUIRES_AUDIO_INPUT = 1u << 3,
    MEDIACAP_REQUIRES_AUDIO_OUTPUT = 1u << 4,
    MEDIACAP_REQUIRES_PIPEWIRE = 1u << 5,
    MEDIACAP_REQUIRES_SCREEN_PERMISSION = 1u << 6,
    MEDIACAP_REQUIRES_CAMERA_PERMISSION = 1u << 7,
    MEDIACAP_REQUIRES_MICROPHONE_PERMISSION = 1u << 8,
    MEDIACAP_REQUIRES_SYSTEM_AUDIO_PERMISSION = 1u << 9,
    MEDIACAP_REQUIRES_PROCESS_ID = 1u << 10,
    MEDIACAP_REQUIRES_FLEXAUDIO = 1u << 11
} MediaCap_Prerequisite;

/** @brief Static requirements for a single feature. These are independent of runtime availability. */
typedef struct {
    MediaCap_Platform platform;
    int32_t included_in_build; /**< Nonzero when the feature's backend was compiled in. */
    uint32_t prerequisites;   /**< Bitmask of MediaCap_Prerequisite values. */
    uint32_t minimum_os_major; /**< Feature-specific OS floor. Zero means no floor is reported. */
    uint32_t minimum_os_minor; /**< Minor version of the reported OS floor. */
    uint32_t minimum_os_build; /**< Build number of the reported floor, or zero when unspecified. */
    char backend[32];          /**< NUL-terminated backend identifier or empty if unknown. */
} MediaCap_FeatureRequirements;

/**
 * @brief Reads static feature requirements without querying devices, services, or consent.
 * Use MediaCap_GetAudioCapabilities() for runtime audio availability. Use the build
 * documentation for deployment targets and binary dependencies.
 * @return 1 on success, including omitted features, or 0 with global diagnostics.
 */
MEDIACAP_API int MediaCap_GetFeatureRequirements(MediaCap_Feature feature, MediaCap_FeatureRequirements* out);

//=================================================================================================
// Session Lifecycle
//=================================================================================================

/**
 * @brief Creates a new, empty recording session.
 *
 * Ownership: the returned handle is owned by the caller and must eventually be passed to
 * MediaCap_Session_Destroy() to free it. Thread-safety: safe to call from any thread; the
 * resulting handle itself is not safe to share across threads for the mutating calls (see
 * MediaCap_Session_Handle).
 * @return A new session handle, or NULL if allocation failed.
 */
MEDIACAP_API MediaCap_Session_Handle MediaCap_Session_Create();

/**
 * @brief Destroys a session, stopping any in-progress recording first.
 *
 * @param handle Session handle from MediaCap_Session_Create(). NULL is a safe no-op. The handle
 * is invalid and must not be used again after this call returns.
 */
MEDIACAP_API void MediaCap_Session_Destroy(MediaCap_Session_Handle handle);

//=================================================================================================
// Device Discovery
//=================================================================================================

/**
 * @brief Enumerates available display monitors.
 *
 * Uses the standard two-call idiom: call once with `buffer` NULL (or `*count` == 0) to get the
 * number of available monitors written to `*count`; allocate an array of that many
 * MediaCap_Monitor elements, then call again with `buffer` pointing at it and `*count` set to its
 * capacity. On return, `*count` holds the number of elements actually written (min of the
 * requested capacity and the number available). Thread-safe/stateless; does not depend on any
 * session handle.
 * Backend discovery can also report an error globally and return an empty/partial inventory;
 * the return value alone does not establish that every device was queried successfully.
 * @param buffer Caller-owned array to fill, or NULL to query the count only. No per-element
 * cleanup is required (MediaCap_Monitor holds no heap pointers).
 * @param count In: capacity of `buffer` (ignored if `buffer` is NULL). Out: number of monitors
 * available/written. Must not be NULL and its input value must be nonnegative, even for a count query.
 * @return 0 on success, -1 if `count` is NULL or an internal error occurred.
 */
MEDIACAP_API int MediaCap_GetMonitors(MediaCap_Monitor* buffer, int* count);

/**
 * @brief Enumerates currently open, visible top-level windows.
 *
 * Same two-call idiom as MediaCap_GetMonitors(); see that function for the buffer/count
 * contract. Thread-safe/stateless.
 * @param buffer Caller-owned array to fill, or NULL to query the count only.
 * @param count In: capacity of `buffer` (ignored if `buffer` is NULL). Out: number of windows
 * available/written. Must not be NULL.
 * @return 0 on success, -1 if `count` is NULL or an internal error occurred.
 */
MEDIACAP_API int MediaCap_GetWindows(MediaCap_Window* buffer, int* count);

/**
 * @brief Lists camera identities without opening devices or querying formats.
 *
 * Uses the same caller-owned buffer/count contract as MediaCap_GetMonitors().
 * IDs use the same ordering as MediaCap_GetCameras(); listed devices may be busy
 * or unavailable for recording. No nested allocations need to be freed.
 * @param buffer Array to fill, or NULL to query the count.
 * @param count Input capacity and output count. Must not be NULL or negative.
 * @return 0 on success, -1 on an invalid argument or internal error.
 */
MEDIACAP_API int MediaCap_GetCameraDevices(MediaCap_Camera* buffer, int* count);

/**
 * @brief Enumerates available cameras along with their supported resolutions/pixel formats.
 *
 * Same two-call idiom as MediaCap_GetMonitors(), but unlike the other Get* functions each
 * populated MediaCap_CameraInfo entry additionally heap-allocates its
 * `supported_resolutions`/`supported_pixel_formats` arrays. The caller **must** call
 * MediaCap_FreeCameraInfoContents() on every populated entry (indices 0 through `*count` - 1)
 * once done with it, or those two arrays will leak. Calling with `buffer` NULL (count-only
 * query) creates no result arrays. Zero-initialize entries before filling and release
 * populated contents before reusing the buffer. Discovery may open devices to query
 * capabilities but does not start video streaming.
 * @param buffer Caller-owned array to fill, or NULL to query the count only.
 * @param count In: capacity of `buffer` (ignored if `buffer` is NULL). Out: number of cameras
 * available/written. Must not be NULL.
 * @return 0 on success, -1 if `count` is NULL or an internal error occurred.
 */
MEDIACAP_API int MediaCap_GetCameras(MediaCap_CameraInfo* buffer, int* count);

/**
 * @brief Frees the heap arrays owned by a single MediaCap_CameraInfo obtained from
 * MediaCap_GetCameras().
 *
 * Frees `info->supported_resolutions` and `info->supported_pixel_formats` and resets both
 * pointers to NULL; does not free `info` itself, which is owned by whoever allocated the
 * MediaCap_CameraInfo array/struct. NULL-safe (`info` == NULL is a no-op), and safe to call more
 * than once on the same struct.
 * @param info The camera info entry to release the contents of.
 */
MEDIACAP_API void MediaCap_FreeCameraInfoContents(MediaCap_CameraInfo* info);

/**
 * @brief Enumerates available audio devices (capture inputs, plus loopback devices where
 * supported by the platform's audio backend).
 *
 * Same two-call idiom as MediaCap_GetMonitors(); see that function for the buffer/count
 * contract. Thread-safe/stateless.
 * @param buffer Caller-owned array to fill, or NULL to query the count only.
 * @param count In: capacity of `buffer` (ignored if `buffer` is NULL). Out: number of devices
 * available/written. Must not be NULL.
 * @return 0 on success, -1 if `count` is NULL or an internal error occurred.
 */
MEDIACAP_API int MediaCap_GetAudioDevices(MediaCap_AudioDevice* buffer, int* count);

//=================================================================================================
// Source Configuration
//=================================================================================================

/**
 * @brief Adds a monitor (screen) capture source to a session.
 *
 * `monitor` and `config` are only read during this call and may be freed/reused immediately
 * afterward. NULL arguments report INVALID_ARGUMENT globally and add nothing. A call while
 * already recording is a no-op. Sources are cleared when the session's recording stops, so
 * add-source calls must be repeated before starting a subsequent recording. Not safe to call
 * concurrently with other mutating calls on the same handle.
 * @param handle Target session.
 * @param monitor The monitor to capture, as returned by MediaCap_GetMonitors().
 * @param config Capture parameters (poll rate) for this source.
 */
MEDIACAP_API void MediaCap_Session_AddMonitorSource(MediaCap_Session_Handle handle, const MediaCap_Monitor* monitor, const MediaCap_ScreenCaptureConfig* config);

/**
 * @brief Adds a window capture source to a session.
 *
 * Same argument-lifetime, no-op, and thread-safety rules as MediaCap_Session_AddMonitorSource().
 * @param handle Target session.
 * @param window The window to capture, as returned by MediaCap_GetWindows().
 * @param config Capture parameters (poll rate) for this source.
 */
MEDIACAP_API void MediaCap_Session_AddWindowSource(MediaCap_Session_Handle handle, const MediaCap_Window* window, const MediaCap_WindowCaptureConfig* config);

/**
 * @brief Adds a camera capture source to a session.
 *
 * Same argument-lifetime, no-op, and thread-safety rules as MediaCap_Session_AddMonitorSource().
 * @param handle Target session.
 * @param camera The camera to capture, as returned by MediaCap_GetCameras().
 * @param config Requested resolution/framerate/pixel format for this source.
 */
MEDIACAP_API void MediaCap_Session_AddCameraSource(MediaCap_Session_Handle handle, const MediaCap_Camera* camera, const MediaCap_CameraCaptureConfig* config);

/**
 * @brief Adds an audio capture source to a session.
 *
 * Same argument-lifetime, no-op, and thread-safety rules as MediaCap_Session_AddMonitorSource().
 * Adding an audio source requires `audio_config` to be set on the
 * MediaCap_RecordingConfig passed to MediaCap_Session_StartRecording(), so the sources can be
 * mixed down to that common format.
 * @param handle Target session.
 * @param device The audio device to capture, as returned by MediaCap_GetAudioDevices().
 * @param format Sample format/rate/channel layout this device should be captured at.
 */
MEDIACAP_API void MediaCap_Session_AddAudioSource(MediaCap_Session_Handle handle, const MediaCap_AudioDevice* device, const MediaCap_AudioFormat* format);

/**
 * @brief Sets per-source raw frame callbacks before starting capture or recording.
 *
 * Either callback can be NULL. The registration persists across starts. Callbacks can run
 * concurrently on capture threads, including during startup and while recording is paused.
 * Audio is delivered before mixing and video before encoding. Indices follow the order in
 * which sources were added. Copy any needed data before returning and return promptly.
 * Do not call mutating session functions from a callback. Stop or destroy the session
 * before releasing user_data. Do not change registration concurrently with session mutations.
 * @param handle Target session.
 * @param audio_callback Listener for captured audio blocks, or NULL to disable audio delivery.
 * @param video_callback Listener for captured video frames, or NULL to disable video delivery.
 * @param user_data Application state passed unchanged to each listener.
 * @return 1 on success, 0 if the handle is invalid or capture is active.
 */
MEDIACAP_API int MediaCap_Session_SetFrameCallbacks(MediaCap_Session_Handle handle,
    MediaCap_AudioFrameCallback audio_callback, MediaCap_VideoFrameCallback video_callback, void* user_data);

/**
 * @brief Starts configured sources for callback delivery without opening a recording file.
 *
 * Requires a callback for at least one configured source. Waits for each source's first
 * frame, up to ten seconds, and uses the session error API on failure. Callbacks can run
 * before this function returns, including on a failed start.
 * @param handle Target session.
 * @return 1 on success or if capture-only mode is already running, 0 on failure.
 */
MEDIACAP_API int MediaCap_Session_StartCapture(MediaCap_Session_Handle handle);

/** @brief Stops capture-only mode, waits for callbacks to return, and clears the source list.
 * @param handle Target session. NULL is a safe no-op.
 */
MEDIACAP_API void MediaCap_Session_StopCapture(MediaCap_Session_Handle handle);

/** @brief Reports whether capture-only mode or recording is active. Safe to poll concurrently.
 * @param handle Target session.
 * @return Nonzero when capture-only mode or recording is active, or zero when idle or handle is NULL.
 */
MEDIACAP_API int MediaCap_Session_IsCapturing(MediaCap_Session_Handle handle);

/**
 * @brief Captures one image from a discovered monitor.
 *
 * Blocks until an image is available or the wait expires. Backend cleanup can take
 * additional time. A NULL config uses defaults. A crop must fit inside the source.
 * If output dimensions are set, both are required and the image is resized to that size.
 * The callback receives borrowed pixels valid only until it returns.
 * @param monitor The monitor to capture, as returned by MediaCap_GetMonitors().
 * @param config Crop, output dimensions and first-frame timeout. NULL uses defaults.
 * @param callback Called synchronously on success. The frame and pixels expire when it returns.
 * @param user_data Application state passed unchanged to the callback.
 * @return 1 when an image was delivered, 0 on failure with a global error.
 */
MEDIACAP_API int MediaCap_CaptureMonitorScreenshot(const MediaCap_Monitor* monitor,
    const MediaCap_ScreenshotConfig* config, MediaCap_ScreenshotCallback callback, void* user_data);

/** @brief Captures one image from a discovered window with the same options as MediaCap_CaptureMonitorScreenshot().
 * @param window The window to capture, as returned by MediaCap_GetWindows().
 * @param config Crop, output dimensions and first-frame timeout. NULL uses defaults.
 * @param callback Called synchronously on success. The frame and pixels expire when it returns.
 * @param user_data Application state passed unchanged to the callback.
 * @return 1 when an image was delivered, 0 on failure with a global error.
 */
MEDIACAP_API int MediaCap_CaptureWindowScreenshot(const MediaCap_Window* window,
    const MediaCap_ScreenshotConfig* config, MediaCap_ScreenshotCallback callback, void* user_data);

//=================================================================================================
// Recording Control
//=================================================================================================

/**
 * @brief Starts recording all previously-added sources to a single output file, with no
 * automatic stop time.
 *
 * `config` (including the pointees of its `video_config`/`audio_config` members and
 * `output_filename`) is only read synchronously during this call; it may be freed/reused
 * immediately after this function returns. If the session is already recording, this is a no-op
 * that returns success. Not safe to call concurrently with other mutating calls on the same
 * handle.
 * Devices are prepared synchronously before recording: wait up to ten seconds for all
 * first frames, then start the file and shared timeline. IsRecording remains false while
 * preparing. Preparation/encoder startup failure stops and clears the sources; backend
 * cleanup may take additional time. Invalid source/encoder combinations fail before preparation.
 * Early validation failure can retain unopened sources; create a fresh session if their state
 * is uncertain before retrying. New non-active start attempts clear the retained session error.
 * Unexpected execution interruption (including sleep) stops recording with DEVICE_LOST at
 * the last continuous cutoff. After stop, re-add sources to record a new file explicitly.
 * @param handle Target session.
 * @param config Output file path, container, and codec configuration.
 * @return 1 on success, 0 on failure (e.g. `handle`/`config` is NULL, an audio source
 * was added without an `audio_config`, a video source lacks `video_config`, a device failed
 * preparation, or the muxer failed to start - unwritable output path,
 * unsupported/unavailable codec, etc.).
 */
MEDIACAP_API int MediaCap_Session_StartRecording(MediaCap_Session_Handle handle, const MediaCap_RecordingConfig* config);

/**
 * @brief Checks the UTF-8 filename, container, active encoder selections, and known audio encoder constraints.
 * No devices or files are opened. Video dimensions can be unspecified for this check.
 * @return 1 on success, 0 on failure with diagnostics through MediaCap_GetLastError().
 */
MEDIACAP_API int MediaCap_ValidateRecordingFormat(const MediaCap_RecordingConfig* config);

/**
 * @brief Starts recording, automatically stopping after a fixed duration.
 *
 * Identical to MediaCap_Session_StartRecording(), except recording is stopped automatically
 * (as if MediaCap_Session_StopRecording() had been called) after `duration_ms` elapses on a
 * background timer thread. `duration_ms` should be a positive value; a value <= 0 will stop the
 * recording almost immediately.
 * @param handle Target session.
 * @param config Output file path, container, and codec configuration.
 * @param duration_ms Elapsed wall duration after preparation in milliseconds, including pauses; paused intervals are omitted from the recorded media.
 * @return 1 on success, 0 on failure; see MediaCap_Session_StartRecording() for failure causes.
 */
MEDIACAP_API int MediaCap_Session_StartRecordingForDuration(MediaCap_Session_Handle handle, const MediaCap_RecordingConfig* config, long long duration_ms);

/**
 * @brief Stops an in-progress recording, finalizes the output file, and clears the session's
 * source list.
 *
 * Blocks until all capture threads have joined and the muxer has finished writing. A no-op if
 * the session is not currently recording (including if `handle` is NULL). Sources must be
 * re-added via the Add*Source functions before starting a new recording on the same session.
 * @param handle Target session.
 */
MEDIACAP_API void MediaCap_Session_StopRecording(MediaCap_Session_Handle handle);

/**
 * @brief Pauses or resumes frame delivery to the muxer without stopping capture threads.
 *
 * A no-op if the session is not currently recording (including if `handle` is NULL).
 * @param handle Target session.
 * @param is_paused Non-zero to pause, zero to resume.
 */
MEDIACAP_API void MediaCap_Session_SetPaused(MediaCap_Session_Handle handle, int is_paused);

/**
 * @brief Checks whether a session is currently recording.
 *
 * Backed by an atomic flag; safe to call concurrently with other calls on the same handle from
 * a different thread.
 * @param handle Target session.
 * @return Non-zero if recording, 0 otherwise (including if `handle` is NULL).
 */
MEDIACAP_API int MediaCap_Session_IsRecording(MediaCap_Session_Handle handle);

/**
 * @brief Checks whether a currently-recording session is paused.
 *
 * Backed by an atomic flag; safe to call concurrently with other calls on the same handle from
 * a different thread.
 * @param handle Target session.
 * @return Non-zero if paused, 0 otherwise (including if `handle` is NULL or not recording).
 */
MEDIACAP_API int MediaCap_Session_IsPaused(MediaCap_Session_Handle handle);

/**
 * @brief Installs a process-wide error listener; NULL disables it.
 * May run concurrently on reporting threads. Copy/queue the record and return promptly;
 * do not mutate/stop/destroy sessions in the callback. Exceptions are contained.
 * Replacement does not wait for in-flight calls; user_data must outlive those calls.
 * @param callback Listener replacing the shared C/C++ slot, or NULL to disable.
 * @param user_data Application state passed unchanged to the callback; never owned by MediaCap.
 */
MEDIACAP_API void MediaCap_SetErrorCallback(MediaCap_ErrorCallback callback, void* user_data);
/** @brief Copies the global latest error; success elsewhere does not clear this record.
 * @param error Caller-owned output; receives a zero-initialized record when none is retained.
 * @return 1 if an error is present, 0 if none or error is NULL.
 */
MEDIACAP_API int MediaCap_GetLastError(MediaCap_Error* error);
/** @brief Clears the global record; session failure records remain intact. */
MEDIACAP_API void MediaCap_ClearLastError(void);
/** @brief Stable identifier for correlating error events, safe to read on a live handle.
 * @param handle Target session, or NULL.
 * @return Session identifier, or zero for a NULL handle.
 */
MEDIACAP_API uint64_t MediaCap_Session_GetId(MediaCap_Session_Handle handle);
/** @brief Checks for a reported recording failure, including after stop; safe to poll concurrently.
 * @param handle Target session, or NULL.
 * @return 1 when a session failure is retained; 0 otherwise, including for NULL.
 */
MEDIACAP_API int MediaCap_Session_HasError(MediaCap_Session_Handle handle);
/**
 * @brief Copies the first session failure, retained until the next start attempt.
 * Returns 1 if present, 0 if none or either argument is NULL. Safe to call concurrently.
 * Check after StopRecording to include finalization failures; failed files may be partial.
 * @param handle Target session, or NULL for a zero-initialized result.
 * @param error Caller-owned output; may be NULL to perform no operation.
 * @return 1 if an error is present; 0 if none or either argument is NULL.
 */
MEDIACAP_API int MediaCap_Session_GetError(MediaCap_Session_Handle handle, MediaCap_Error* error);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MEDIACAP_C_API_H
