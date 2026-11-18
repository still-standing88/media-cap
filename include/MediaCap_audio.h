/** @file MediaCap_audio.h
 * @brief Optional flexaudio routing, device/process discovery and runtime capabilities.
 */
#ifndef MEDIACAP_AUDIO_H
#define MEDIACAP_AUDIO_H
#include "MediaCap_types.h"
#include "MediaCap_export.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Audio routing supported by the optional flexaudio backend. */
typedef enum {
    MEDIACAP_AUDIO_CAPTURE_MIC = 0,       /**< Default or selected input device. */
    MEDIACAP_AUDIO_CAPTURE_SYSTEM,        /**< Default or selected system output. */
    MEDIACAP_AUDIO_CAPTURE_PROCESS,       /**< Audio from/excluding a target process. */
    MEDIACAP_AUDIO_CAPTURE_MIX            /**< Microphone and system output combined. */
} MediaCap_AudioCaptureKind;
/** @brief Process include/exclude mode; scope is platform-dependent (see capabilities). */
typedef enum { MEDIACAP_PROCESS_INCLUDE = 0, /**< Capture the target. */
    MEDIACAP_PROCESS_EXCLUDE /**< Capture all output except the target. */
} MediaCap_ProcessAudioMode;
/** @brief Runtime capability availability, independent of recording consent. */
typedef enum { MEDIACAP_AUDIO_UNAVAILABLE = 0, /**< Unsupported prerequisite/build. */
    MEDIACAP_AUDIO_AVAILABLE, /**< Platform supports the feature. */
    MEDIACAP_AUDIO_AVAILABILITY_UNKNOWN /**< Prerequisite query failed. */
} MediaCap_AudioAvailability;
/** @brief Scope of process identifiers on this platform. */
typedef enum { MEDIACAP_PID_SCOPE_NONE = 0, /**< No applicable operation. */
    MEDIACAP_PID_SCOPE_EXACT, /**< Only the specified PID. */
    MEDIACAP_PID_SCOPE_TREE /**< PID and descendants. */
} MediaCap_AudioPidScope;
/** @brief Initialize with MediaCap_DefaultAudioSourceConfig; pointer contents are copied by AddAudioCaptureSource. */
typedef struct {
    MediaCap_AudioCaptureKind kind;       /**< Input/system/process/mix. */
    const char* device_id;               /**< UTF-8 ID from audio capture discovery; NULL selects default. */
    uint32_t process_id;                 /**< Required nonzero target for PROCESS. */
    MediaCap_ProcessAudioMode process_mode; /**< Include/exclude the target. */
    int exclude_self;                    /**< Exclude this application's output from SYSTEM/MIX. */
    const uint32_t* exclude_pids;         /**< Additional exclusions for SYSTEM/MIX, or NULL. */
    size_t exclude_pid_count;            /**< Length of exclude_pids. */
    const char* mix_mic_device_id;        /**< Input selection for MIX, or NULL for default. */
    const char* mix_system_device_id;     /**< Output selection for MIX, or NULL for default. */
    float gain;                          /**< Linear output gain; zero mutes. */
    float mix_mic_gain;                   /**< MIX input gain. */
    float mix_system_gain;                /**< MIX output gain. */
    uint32_t sample_rate;                /**< Output rate; zero uses flexaudio's default. */
    uint16_t channels;                   /**< Mono/stereo; zero uses flexaudio's default. */
    size_t ring_capacity_chunks;          /**< Bounded capture queue; zero uses flexaudio's default. */
} MediaCap_AudioSourceConfig;
/** @brief Owned device entry returned by GetAudioCaptureDevices; IDs are distinct from miniaudio IDs. */
typedef struct {
    char* id;                            /**< Selectable UTF-8 ID; do not truncate or reinterpret. */
    char* name;                          /**< Display name. */
    MediaCap_AudioCaptureKind kind;       /**< MIC or SYSTEM. */
    uint32_t sample_rate;                /**< Native rate, zero if unknown. */
    uint16_t channels;                   /**< Native channels, zero if unknown. */
    int is_default;                      /**< Whether this is the default input/output. */
} MediaCap_AudioCaptureDevice;
/** @brief Owned process entry; enumeration includes idle audio sessions. */
typedef struct {
    uint32_t pid;                        /**< Runtime process identity. */
    char* name;                          /**< Display name. */
    char* executable;                    /**< Executable basename, or NULL when unavailable. */
    int output_active;                   /**< -1 unknown, 0 inactive, 1 active. */
} MediaCap_AudioProcess;
/** @brief Runtime capability snapshot; reading it does not start recording. */
typedef struct {
    int backend_enabled;                 /**< flexaudio was included in this build. */
    MediaCap_AudioAvailability microphone; /**< Input capture support. */
    MediaCap_AudioAvailability system;    /**< System output support. */
    MediaCap_AudioAvailability process;   /**< Per-process support. */
    MediaCap_AudioAvailability mix;       /**< Combined input/output support. */
    MediaCap_AudioAvailability system_exclusions; /**< Exclusion routing support. */
    MediaCap_AudioPidScope process_scope; /**< Target PID matching semantics. */
    MediaCap_AudioPidScope exclusion_scope; /**< Excluded PID matching semantics. */
    int exclusion_honors_device;          /**< Exclusions can retain a selected output device. */
    uint32_t max_excluded_pids;           /**< Maximum exclusion count, or zero when unlimited. */
    char process_detail[256];            /**< Support/query diagnostic for process capture. */
    char system_detail[256];             /**< Support/query diagnostic for system capture. */
} MediaCap_AudioCapabilities;
/** @brief Default configuration with unity gains and default input routing.
 * @return MIC routing with unity gains, no exclusions and backend-default rate/channels/queue.
 */
MEDIACAP_API MediaCap_AudioSourceConfig MediaCap_DefaultAudioSourceConfig(void);
/** @brief Copies runtime support without opening capture or granting OS permission.
 * Disabled builds succeed with backend_enabled equal to zero. Availability is not a
 * guarantee that a particular source can open; preparation is the final check.
 * @param out Caller-owned capability output; must not be NULL.
 * @return 1 on success, 0 on query/argument failure with global diagnostics.
 */
MEDIACAP_API int MediaCap_GetAudioCapabilities(MediaCap_AudioCapabilities* out);
/** @brief Allocates an inventory of flexaudio input/output devices and their UTF-8 strings.
 * This is a single allocation-returning call, unlike the classic two-call discovery API.
 * Release with MediaCap_FreeAudioCaptureDevices, including successful empty inventories.
 * @param out Required output pointer; set to NULL on failure. Do not overwrite a live inventory.
 * @param count Required output count; set to zero on failure.
 * @return 1 on success, including an empty inventory; 0 on failure with global diagnostics.
 */
MEDIACAP_API int MediaCap_GetAudioCaptureDevices(MediaCap_AudioCaptureDevice** out, size_t* count);
/** @brief Frees an entire inventory and its strings, exactly once; NULL is safe.
 * @param devices Array returned by MediaCap_GetAudioCaptureDevices, or NULL.
 * @param count Exact returned count; do not alter it before releasing the array.
 */
MEDIACAP_API void MediaCap_FreeAudioCaptureDevices(MediaCap_AudioCaptureDevice* devices, size_t count);
/** @brief Allocates process candidates, including idle sessions where the OS reports them.
 * Presence does not promise audible media. Release using MediaCap_FreeAudioProcesses.
 * @param out Required output pointer; set to NULL on failure. Do not overwrite a live inventory.
 * @param count Required output count; set to zero on failure.
 * @return 1 on success, including an empty inventory; 0 on failure with global diagnostics.
 */
MEDIACAP_API int MediaCap_GetAudioProcesses(MediaCap_AudioProcess** out, size_t* count);
/** @brief Frees an entire process inventory, exactly once; NULL is safe.
 * @param processes Array returned by MediaCap_GetAudioProcesses, or NULL.
 * @param count Exact returned count.
 */
MEDIACAP_API void MediaCap_FreeAudioProcesses(MediaCap_AudioProcess* processes, size_t count);
/** @brief Adds a flexaudio source; copies config and all referenced data without opening capture.
 * Initialize config with MediaCap_DefaultAudioSourceConfig. Unsupported/inapplicable
 * settings fail instead of silently broadening scope. Cannot add while recording.
 * @param session Target session; must not be NULL.
 * @param config Source settings, including strings/arrays valid until this call returns.
 * @return 1 when added, 0 on error; use session/global diagnostics for the reason.
 */
MEDIACAP_API int MediaCap_Session_AddAudioCaptureSource(MediaCap_Session_Handle session, const MediaCap_AudioSourceConfig* config);
#ifdef __cplusplus
}
#endif
#endif
