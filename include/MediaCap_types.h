/** @file MediaCap_types.h
 * @brief Shared C data structures, enums and C++ aliases.
 */
#ifndef MEDIACAP_TYPES_H
#define MEDIACAP_TYPES_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
#include <memory>
#include <vector>
#include <variant>
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // To avoid including the full miniaudio.h header, we define a sufficiently
    // large buffer to hold the platform-specific ma_device_id structure.
/** @brief Storage size for an opaque classic audio device ID; pass its bytes back unchanged. */
#define MEDIACAP_MA_DEVICE_ID_SIZE 256

//=================================================================================================
// Forward Declarations & Opaque Handles for the C API
//=================================================================================================

    /**
     * @brief Opaque handle to a capture/recording session created by MediaCap_Session_Create().
     *
     * The pointed-to type is never defined in the public API; callers must treat this as an
     * opaque token. Ownership: the handle is owned by the caller until it is passed to
     * MediaCap_Session_Destroy(), which frees it. A handle must not be used from more than one
     * thread at a time for the mutating calls (AddSource/StartRecording/StopRecording/SetPaused);
     * MediaCap_Session_IsRecording()/MediaCap_Session_IsPaused() are safe to call concurrently
     * with those.
     */
    typedef struct MediaCap_Session_t* MediaCap_Session_Handle;

    //=================================================================================================
    // General Enums
    //=================================================================================================

    /**
     * @brief Identifies which kind of media source a Capturer/handle represents.
     */
    typedef enum {
        MEDIACAP_SOURCE_TYPE_UNKNOWN = 0,      /**< No source type has been assigned yet. */
        MEDIACAP_SOURCE_TYPE_MONITOR,          /**< A full display monitor (screen capture). */
        MEDIACAP_SOURCE_TYPE_WINDOW,           /**< A single application window. */
        MEDIACAP_SOURCE_TYPE_CAMERA,           /**< A camera/webcam device. */
        MEDIACAP_SOURCE_TYPE_AUDIO_INPUT,      /**< A microphone/line-in capture device. */
        MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT,     /**< A loopback capture of an audio playback device. */
        MEDIACAP_SOURCE_TYPE_AUDIO_PROCESS,    /**< Per-process audio capture. */
        MEDIACAP_SOURCE_TYPE_AUDIO_MIX         /**< Combined microphone/system audio capture. */
    } MediaCap_SourceType;

    /** @brief Stable failure classes shared by capture, processing, and API operations. */
    typedef enum {
        MEDIACAP_ERROR_NONE = 0,              /**< No error. */
        MEDIACAP_ERROR_INVALID_ARGUMENT,      /**< Invalid configuration or API argument. */
        MEDIACAP_ERROR_DEVICE_UNAVAILABLE,    /**< Device/window could not be opened or found. */
        MEDIACAP_ERROR_DEVICE_LOST,           /**< Active source loss/stall or interrupted session execution. */
        MEDIACAP_ERROR_TIMEOUT,               /**< Source readiness or backend cleanup timed out. */
        MEDIACAP_ERROR_BUFFER_OVERFLOW,       /**< A bounded queue lost media. */
        MEDIACAP_ERROR_CAPTURE_FAILED,        /**< Capture backend failed. */
        MEDIACAP_ERROR_ENCODING_FAILED,       /**< Conversion, codec, or muxing failed. */
        MEDIACAP_ERROR_IO,                    /**< Output file could not be opened/written. */
        MEDIACAP_ERROR_INTERNAL,              /**< Unexpected exception or internal failure. */
        MEDIACAP_ERROR_UNSUPPORTED,           /**< Feature/backend unavailable on this build or platform. */
        MEDIACAP_ERROR_PERMISSION_DENIED      /**< The operating system denied capture access. */
    } MediaCap_ErrorCode;

    /** @brief Library subsystem that reported a failure. */
    typedef enum {
        MEDIACAP_ERROR_COMPONENT_API = 0,     /**< Public API validation/exception boundary. */
        MEDIACAP_ERROR_COMPONENT_DISCOVERY,   /**< Device enumeration. */
        MEDIACAP_ERROR_COMPONENT_SESSION,     /**< Recording lifecycle/readiness. */
        MEDIACAP_ERROR_COMPONENT_CAPTURE,     /**< Camera, screen, window, or audio acquisition. */
        MEDIACAP_ERROR_COMPONENT_MIXER,       /**< Audio conversion/mixing. */
        MEDIACAP_ERROR_COMPONENT_ENCODER      /**< Encoding and output muxing. */
    } MediaCap_ErrorComponent;

    /** @brief Copyable error record; no borrowed strings or device pointers. */
    typedef struct {
        MediaCap_ErrorCode code;             /**< Machine-readable failure class. */
        MediaCap_ErrorComponent component;   /**< Subsystem reporting the error. */
        MediaCap_SourceType source_type;     /**< Source kind, or UNKNOWN for a session/output error. */
        int source_index;                    /**< Position in the session's added sources, or -1. */
        uint64_t session_id;                 /**< Session::id()/GetId(), or zero for a global operation. */
        int64_t native_code;                 /**< Backend error code if available, otherwise zero. */
        char message[512];                   /**< NUL-terminated diagnostic, possibly truncated. */
    } MediaCap_Error;

    /** @brief Global error listener; the record is valid only during the call. */
    typedef void (*MediaCap_ErrorCallback)(const MediaCap_Error* error, void* user_data);

    /**
     * @brief Logging verbosity levels, ordered from least to most verbose.
     *
     * @note There is currently no public logging-level setter. Use the error listener
     * to receive structured failures; this enum does not control backend log output.
     */
    typedef enum {
        MEDIACAP_LOG_LEVEL_NONE = 0,    /**< Logging disabled. */
        MEDIACAP_LOG_LEVEL_ERROR,       /**< Only unrecoverable errors. */
        MEDIACAP_LOG_LEVEL_WARNING,     /**< Errors and recoverable warnings. */
        MEDIACAP_LOG_LEVEL_INFO,        /**< Warnings plus high-level informational messages. */
        MEDIACAP_LOG_LEVEL_DEBUG,       /**< Info plus developer-oriented debug messages. */
        MEDIACAP_LOG_LEVEL_VERBOSE      /**< Everything, including high-frequency trace output. */
    } MediaCap_LogLevel;

    //=================================================================================================
    // Video Related Structs and Enums
    //=================================================================================================

    /**
     * @brief Raw pixel formats used for video frames.
     *
     * The numeric values intentionally match FFmpeg's `AVPixelFormat` enum (e.g.
     * `MEDIACAP_PIXEL_FORMAT_I420` == `AV_PIX_FMT_YUV420P` == 0) for convenience when cross-
     * referencing with FFmpeg, but call sites still convert explicitly via a switch (see
     * `FFmpegMuxer.cpp`/`CameraCapturer.cpp`) rather than relying on the numeric match.
     */
    typedef enum {
        MEDIACAP_PIXEL_FORMAT_UNKNOWN = -1,    /**< Format not recognized/not yet determined. */
        MEDIACAP_PIXEL_FORMAT_BGRA = 28,       /**< 32-bit packed BGRA, 8 bits per channel. */
        MEDIACAP_PIXEL_FORMAT_BGR24 = 3,       /**< 24-bit packed BGR, 8 bits per channel. */
        MEDIACAP_PIXEL_FORMAT_RGBA = 26,       /**< 32-bit packed RGBA, 8 bits per channel. */
        MEDIACAP_PIXEL_FORMAT_RGB24 = 2,       /**< 24-bit packed RGB, 8 bits per channel. */
        MEDIACAP_PIXEL_FORMAT_NV12 = 23,       /**< 8-bit 4:2:0, one luma plane + interleaved chroma plane. */
        MEDIACAP_PIXEL_FORMAT_I420 = 0,        /**< 8-bit 4:2:0 planar (Y, U, V each in their own plane). */
    } MediaCap_PixelFormat;

    /**
     * @brief Vertical orientation of the rows in a MediaCap_VideoFrame's plane data.
     */
    typedef enum {
        MEDIACAP_ORIENTATION_TOP_DOWN = 0,     /**< First row in the buffer is the top of the image. */
        MEDIACAP_ORIENTATION_BOTTOM_UP         /**< First row in the buffer is the bottom of the image. */
    } MediaCap_FrameOrientation;

    /**
     * @brief A simple width/height pair, in pixels.
     */
    typedef struct {
        uint32_t width;     /**< Width in pixels. */
        uint32_t height;    /**< Height in pixels. */
    } MediaCap_Resolution;

    /**
     * @brief Describes a single display monitor as returned by MediaCap_GetMonitors()/get_monitors().
     */
    typedef struct {
        int32_t id;         /**< Backend monitor identifier; refresh discovery after topology changes. */
        int32_t index;      /**< Zero-based enumeration index at the time of the query (not stable across calls). */
        int32_t offsetX;    /**< X offset of the monitor's top-left corner in the virtual desktop, in pixels. */
        int32_t offsetY;    /**< Y offset of the monitor's top-left corner in the virtual desktop, in pixels. */
        uint32_t width;     /**< Monitor width in pixels. */
        uint32_t height;    /**< Monitor height in pixels. */
        float scaling;      /**< Display/DPI scaling factor (1.0 = 100%). */
        char name[128];     /**< Null-terminated, human-readable monitor name. */
    } MediaCap_Monitor;

    /**
     * @brief Describes a single top-level application window as returned by MediaCap_GetWindows()/get_windows().
     */
    typedef struct {
        uint64_t handle;    /**< Platform-specific native window handle (e.g. HWND on Windows), stored as an integer. */
        int32_t offsetX;    /**< X offset of the window's top-left corner on screen, in pixels. */
        int32_t offsetY;    /**< Y offset of the window's top-left corner on screen, in pixels. */
        uint32_t width;     /**< Window width in pixels. */
        uint32_t height;    /**< Window height in pixels. */
        char name[256];     /**< Null-terminated window title. */
    } MediaCap_Window;

    /**
     * @brief Identifies a single camera device.
     */
    typedef struct {
        int32_t index;          /**< Zero-based enumeration index; valid only for the current enumeration pass. */
        char name[256];         /**< Null-terminated, human-readable camera name. */
        char unique_id[256];    /**< Reserved device identity storage; current discovery can leave it empty. */
    } MediaCap_Camera;

    /**
     * @brief A camera plus its supported resolutions/pixel formats, as returned by MediaCap_GetCameras().
     *
     * Ownership: `supported_resolutions` and `supported_pixel_formats` are heap-allocated by
     * MediaCap_GetCameras() with `new[]`. The caller owns them and **must** call
     * MediaCap_FreeCameraInfoContents() on every populated entry once done with it to avoid
     * leaking those two arrays; this struct itself is not freed by that call (it is owned by
     * whoever allocated the MediaCap_CameraInfo array/struct).
     */
    typedef struct {
        MediaCap_Camera camera;                            /**< The underlying camera identity. */
        MediaCap_Resolution* supported_resolutions;         /**< Caller-owned array of supported resolutions; see ownership note above. */
        size_t num_supported_resolutions;                   /**< Number of entries in `supported_resolutions`. */
        MediaCap_PixelFormat* supported_pixel_formats;      /**< Caller-owned array of supported pixel formats; see ownership note above. */
        size_t num_supported_pixel_formats;                 /**< Number of entries in `supported_pixel_formats`. */
    } MediaCap_CameraInfo;

    /**
     * @brief A single decoded video frame.
     *
     * Ownership: this struct never owns the memory pointed to by `data` - it is a plain,
     * non-owning view (mirroring FFmpeg's plane layout). Internal capture callbacks carry
     * a shared owner for the storage; copying this struct alone does not retain pixels.
     * Public callbacks expose the read-only MediaCap_VideoFrameView.
     */
    typedef struct {
        uint8_t* data[4];                          /**< Per-plane data pointers (up to 4 planes); unused planes are left null. */
        int32_t linesize[4];                       /**< Per-plane stride in bytes, parallel to `data`. */
        uint32_t width;                             /**< Frame width in pixels. */
        uint32_t height;                            /**< Frame height in pixels. */
        MediaCap_PixelFormat pixel_format;          /**< Pixel format describing how `data`/`linesize` should be interpreted. */
        MediaCap_FrameOrientation orientation;      /**< Row order of the plane data. */
        int64_t timestamp_ns;                       /**< Presentation timestamp in nanoseconds on the shared recording timeline; paused intervals are omitted. Internal capturers use host-clock timestamps before session mapping. */
        uint64_t frame_index;                       /**< Zero-based sequence number of this frame within its capture session. */
    } MediaCap_VideoFrame;

    /**
     * @brief Requested capture parameters for a camera source.
     */
    typedef struct {
        uint32_t width;                     /**< Requested capture width in pixels. */
        uint32_t height;                    /**< Requested capture height in pixels. */
        int32_t framerate;                  /**< Requested capture framerate in frames per second. */
        MediaCap_PixelFormat pixel_format;  /**< Requested pixel format for delivered frames. */
    } MediaCap_CameraCaptureConfig;

    /**
     * @brief Requested capture parameters for a monitor (screen) source.
     */
    typedef struct {
        int32_t capture_rate_ms;    /**< Polling interval between captured frames, in milliseconds. */
    } MediaCap_ScreenCaptureConfig;

    /**
     * @brief Requested capture parameters for a window source.
     */
    typedef struct {
        int32_t capture_rate_ms;    /**< Polling interval between captured frames, in milliseconds. */
    } MediaCap_WindowCaptureConfig;

    //=================================================================================================
    // Audio Related Structs and Enums
    //=================================================================================================

    /**
     * @brief The role/direction of an audio device.
     */
    typedef enum {
        MEDIACAP_AUDIO_DEVICE_TYPE_UNKNOWN = 0,    /**< Device type has not been determined. */
        MEDIACAP_AUDIO_DEVICE_TYPE_INPUT,          /**< A capture (recording) device, e.g. a microphone. */
        MEDIACAP_AUDIO_DEVICE_TYPE_OUTPUT,         /**< A playback device, e.g. speakers. */
        MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK        /**< A playback device captured in loopback mode ("what you hear"). */
    } MediaCap_AudioDeviceType;

    /**
     * @brief Sample formats for raw audio buffers.
     *
     * `*P` suffixed values are planar (one buffer per channel); the non-suffixed values are
     * packed/interleaved. In practice, MediaCap's built-in audio capturers currently only
     * produce interleaved data (see MediaCap_AudioFrame::data).
     */
    typedef enum {
        MEDIACAP_SAMPLE_FORMAT_UNKNOWN = -1,   /**< Format not recognized/not yet determined. */
        MEDIACAP_SAMPLE_FORMAT_U8 = 0,         /**< Packed unsigned 8-bit. */
        MEDIACAP_SAMPLE_FORMAT_S16 = 1,        /**< Packed signed 16-bit. */
        MEDIACAP_SAMPLE_FORMAT_S32 = 2,        /**< Packed signed 32-bit. */
        MEDIACAP_SAMPLE_FORMAT_FLT = 3,        /**< Packed 32-bit float. */
        MEDIACAP_SAMPLE_FORMAT_DBL = 4,        /**< Packed 64-bit float (double). */
        MEDIACAP_SAMPLE_FORMAT_U8P = 5,        /**< Planar unsigned 8-bit. */
        MEDIACAP_SAMPLE_FORMAT_S16P = 6,       /**< Planar signed 16-bit. */
        MEDIACAP_SAMPLE_FORMAT_S32P = 7,       /**< Planar signed 32-bit. */
        MEDIACAP_SAMPLE_FORMAT_FLTP = 8,       /**< Planar 32-bit float. */
        MEDIACAP_SAMPLE_FORMAT_DBLP = 9,       /**< Planar 64-bit float (double). */
    } MediaCap_SampleFormat;

    /**
     * @brief Describes a single audio input/output/loopback device.
     */
    typedef struct {
        uint8_t id[MEDIACAP_MA_DEVICE_ID_SIZE];    /**< Opaque, platform-specific device identifier (raw copy of miniaudio's ma_device_id); pass back unmodified to select this device. */
        char name[256];                             /**< Null-terminated, human-readable device name. */
        MediaCap_AudioDeviceType type;               /**< Whether this is an input, output, or loopback device. */
        int is_default;                              /**< Non-zero if this is the platform's default device for its type. */
    } MediaCap_AudioDevice;

    /**
     * @brief Describes the sample layout of an audio stream.
     */
    typedef struct {
        MediaCap_SampleFormat format;   /**< Sample format of the audio data. */
        uint32_t sample_rate;           /**< Sample rate in Hz. */
        uint8_t channels;                /**< Number of interleaved audio channels. */
    } MediaCap_AudioFormat;

    /**
     * @brief A single block of captured audio samples.
     *
     * Ownership: this struct never owns the memory pointed to by `data` - see the ownership note
     * on MediaCap_VideoFrame; the same non-owning/shared_ptr-custodian model applies here.
     */
    typedef struct {
        uint8_t** data;                    /**< Per-plane sample data pointers. For the interleaved/packed sample formats currently produced by MediaCap's capturers, only `data[0]` is populated. */
        uint32_t num_samples;               /**< Number of sample frames in this block (i.e. per-channel sample count, not samples * channels). */
        MediaCap_AudioFormat format;         /**< Sample format/rate/channel layout describing `data`. */
        int64_t timestamp_ns;                /**< Presentation timestamp in nanoseconds on the shared recording timeline; paused intervals are omitted. Internal capturers use host-clock timestamps before session mapping. */
    } MediaCap_AudioFrame;

    /** @brief Borrowed, read-only video data. All pointers expire when the callback returns. */
    typedef struct {
        const uint8_t* data[4];                 /**< Pixel planes; unused planes are null. */
        int32_t linesize[4];                    /**< Byte stride of each plane. */
        uint32_t width;                         /**< Frame width in pixels. */
        uint32_t height;                        /**< Frame height in pixels. */
        MediaCap_PixelFormat pixel_format;      /**< Pixel layout. */
        MediaCap_FrameOrientation orientation;  /**< Row order. */
        int64_t timestamp_ns;                   /**< Monotonic host-clock timestamp in nanoseconds. */
        uint64_t frame_index;                   /**< Zero-based frame sequence for the source. */
    } MediaCap_VideoFrameView;

    /** @brief Borrowed, read-only interleaved audio. The data expires when the callback returns. */
    typedef struct {
        const uint8_t* data;                    /**< Interleaved PCM bytes. */
        uint32_t num_samples;                   /**< Number of sample frames per channel. */
        MediaCap_AudioFormat format;            /**< Sample format, rate, and channel count. */
        int64_t timestamp_ns;                   /**< Monotonic host-clock timestamp in nanoseconds. */
    } MediaCap_AudioFrameView;

    /** @brief Settings for a one-shot monitor or window image. All zeros select defaults. */
    typedef struct {
        uint32_t x;                             /**< Crop origin relative to the source's left edge. */
        uint32_t y;                             /**< Crop origin relative to the source's top edge. */
        uint32_t width;                         /**< Crop width; both crop dimensions zero select the full source. */
        uint32_t height;                        /**< Crop height. */
        uint32_t output_width;                  /**< Output width; both output dimensions zero keep the crop size. */
        uint32_t output_height;                 /**< Output height. */
        uint32_t timeout_ms;                    /**< First-frame wait limit; zero uses ten seconds. */
    } MediaCap_ScreenshotConfig;

    /** @brief Receives an audio block with its zero-based source index and type. */
    typedef void (*MediaCap_AudioFrameCallback)(size_t source_index, MediaCap_SourceType source_type,
        const MediaCap_AudioFrameView* frame, void* user_data);

    /** @brief Receives a video frame with its zero-based source index and type. */
    typedef void (*MediaCap_VideoFrameCallback)(size_t source_index, MediaCap_SourceType source_type,
        const MediaCap_VideoFrameView* frame, void* user_data);

    /** @brief Receives a top-down BGRA screenshot on the calling thread. */
    typedef void (*MediaCap_ScreenshotCallback)(const MediaCap_VideoFrameView* frame, void* user_data);

    //=================================================================================================
    // Encoding & Muxing Related Structs and Enums
    //=================================================================================================

    /**
     * @brief Output container/muxer format. MEDIACAP_CONTAINER_DEFAULT lets the muxer infer the
     * container from the output filename's extension.
     */
    typedef enum {
        MEDIACAP_CONTAINER_DEFAULT = 0,    /**< Infer the container from the output filename extension. */
        MEDIACAP_CONTAINER_MP4,            /**< MP4 container. */
        MEDIACAP_CONTAINER_MKV,            /**< Matroska container. */
        MEDIACAP_CONTAINER_WEBM,           /**< WebM container. */
        MEDIACAP_CONTAINER_M4A,            /**< M4A (audio-only MP4) container. */
        MEDIACAP_CONTAINER_FLAC,           /**< Raw FLAC container. */
        MEDIACAP_CONTAINER_MP3,            /**< Raw MP3 container. */
        MEDIACAP_CONTAINER_WAV,            /**< WAV container. */
        MEDIACAP_CONTAINER_OPUS,           /**< Ogg Opus container. */
    } MediaCap_ContainerFormat;

    /**
     * @brief Video codec selection. MEDIACAP_VIDEO_CODEC_DEFAULT lets the muxer choose a codec
     * appropriate for the selected container.
     */
    typedef enum {
        MEDIACAP_VIDEO_CODEC_DEFAULT = 0,  /**< Choose a codec appropriate for the container. */
        MEDIACAP_VIDEO_CODEC_H264,         /**< H.264/AVC (encoded via libopenh264). */
        MEDIACAP_VIDEO_CODEC_VP9,          /**< VP9 (encoded via libvpx-vp9). */
        MEDIACAP_VIDEO_CODEC_AV1,          /**< AV1 (encoded via libaom-av1). */
    } MediaCap_VideoCodec;

    /**
     * @brief Audio codec selection. MEDIACAP_AUDIO_CODEC_DEFAULT lets the muxer choose a codec
     * appropriate for the selected container.
     */
    typedef enum {
        MEDIACAP_AUDIO_CODEC_DEFAULT = 0,  /**< Choose a codec appropriate for the container. */
        MEDIACAP_AUDIO_CODEC_AAC,          /**< AAC (FFmpeg's built-in encoder). */
        MEDIACAP_AUDIO_CODEC_OPUS,         /**< Opus (encoded via libopus). */
        MEDIACAP_AUDIO_CODEC_VORBIS,       /**< Vorbis (encoded via libvorbis). */
        MEDIACAP_AUDIO_CODEC_FLAC,         /**< FLAC (lossless). */
        MEDIACAP_AUDIO_CODEC_MP3,          /**< MP3 (encoded via libmp3lame). */
        MEDIACAP_AUDIO_CODEC_PCM,          /**< Uncompressed PCM (pcm_s16le). */
    } MediaCap_AudioCodec;

    /**
     * @brief Video encoder settings for a recording.
     */
    typedef struct {
        uint32_t width;              /**< Output width in pixels (rounded down to an even number internally). */
        uint32_t height;             /**< Output height in pixels (rounded down to an even number internally). */
        int32_t framerate;           /**< Output framerate in frames per second; also used as the stream's time base denominator. */
        int64_t bitrate_bps;         /**< Target video bitrate in bits per second. */
        MediaCap_VideoCodec codec;   /**< Video codec to encode with. */
    } MediaCap_VideoEncoderConfig;

    /**
     * @brief Audio encoder settings for a recording.
     */
    typedef struct {
        uint32_t sample_rate;        /**< Output sample rate in Hz. */
        uint8_t channels;             /**< Output channel count. */
        int64_t bitrate_bps;          /**< Target audio bitrate in bits per second. */
        MediaCap_AudioCodec codec;    /**< Audio codec to encode with. */
    } MediaCap_AudioEncoderConfig;

    /**
     * @brief Top-level configuration for a recording, passed to Session::start_recording()/
     * MediaCap_Session_StartRecording().
     *
     * Ownership/lifetime: `output_filename` and the pointees of `video_config`/`audio_config`
     * are only read synchronously while starting the recording (the muxer copies what it needs
     * into its own storage); none of these pointers need to remain valid after the
     * start-recording call returns. Set `video_config`/`audio_config` to null to omit that
     * stream entirely. At least one encoder configuration is required, and every added
     * source needs the corresponding media-kind configuration. Missing configurations and
     * invalid output dimensions/rates are rejected. Existing output files may be overwritten.
     */
    typedef struct {
        const char* output_filename;                    /**< NUL-terminated UTF-8 file path; valid for the duration of the start-recording call. */
        MediaCap_ContainerFormat container;              /**< Output container format. */
        MediaCap_VideoEncoderConfig* video_config;       /**< Video encoder settings, or null to record audio-only. */
        MediaCap_AudioEncoderConfig* audio_config;       /**< Audio encoder settings, or null to record video-only. */
    } MediaCap_RecordingConfig;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace MediaCap {

    // C++ wrappers for easier use
    using Resolution = MediaCap_Resolution; /**< Width/height value. */
    using Monitor = MediaCap_Monitor; /**< Monitor descriptor. */
    using Window = MediaCap_Window; /**< Window descriptor. */
    using Camera = MediaCap_Camera; /**< Camera descriptor. */
    using CameraCaptureConfig = MediaCap_CameraCaptureConfig; /**< Camera capture request. */
    using AudioDevice = MediaCap_AudioDevice; /**< Classic audio descriptor. */
    using VideoEncoderConfig = MediaCap_VideoEncoderConfig; /**< Video output settings. */
    using AudioEncoderConfig = MediaCap_AudioEncoderConfig; /**< Audio output settings. */
    using RecordingConfig = MediaCap_RecordingConfig; /**< File and encoder settings. */
    using ScreenCaptureConfig = MediaCap_ScreenCaptureConfig; /**< Monitor polling settings. */
    using WindowCaptureConfig = MediaCap_WindowCaptureConfig; /**< Window polling settings. */

    /**
     * @brief Variant that holds either an audio or a video frame; this is the payload passed
     * between capture and encoding threads.
     *
     * The frame structs do not own their buffers. Internal users must retain a separate
     * storage owner (for example through an aliasing shared_ptr). Copying this variant
     * alone does not extend buffer lifetime. No public raw-frame callback uses this type.
     */
    using PackedFrame = std::variant<MediaCap_AudioFrame, MediaCap_VideoFrame>;

} // namespace MediaCap

#endif // __cplusplus

#endif // MEDIACAP_TYPES_H
