#include "binding.hpp"

namespace mediacap::python {
void bind_enums(py::module_& module) {
    py::enum_<MediaCap_Platform>(module, "Platform")
        .value("UNKNOWN", MEDIACAP_PLATFORM_UNKNOWN)
        .value("WINDOWS", MEDIACAP_PLATFORM_WINDOWS)
        .value("MACOS", MEDIACAP_PLATFORM_MACOS)
        .value("LINUX", MEDIACAP_PLATFORM_LINUX);
    py::enum_<MediaCap_Feature>(module, "Feature")
        .value("SCREEN", MEDIACAP_FEATURE_SCREEN)
        .value("WINDOW", MEDIACAP_FEATURE_WINDOW)
        .value("CAMERA", MEDIACAP_FEATURE_CAMERA)
        .value("MICROPHONE", MEDIACAP_FEATURE_MICROPHONE)
        .value("SYSTEM_AUDIO", MEDIACAP_FEATURE_SYSTEM_AUDIO)
        .value("PROCESS_AUDIO", MEDIACAP_FEATURE_PROCESS_AUDIO)
        .value("MIX_AUDIO", MEDIACAP_FEATURE_MIX_AUDIO)
        .value("AUDIO_EXCLUSION", MEDIACAP_FEATURE_AUDIO_EXCLUSION)
        .value("CLASSIC_AUDIO", MEDIACAP_FEATURE_CLASSIC_AUDIO);
    py::enum_<MediaCap_Prerequisite>(module, "Prerequisite")
        .value("DISPLAY_SESSION", MEDIACAP_REQUIRES_DISPLAY_SESSION)
        .value("X11", MEDIACAP_REQUIRES_X11)
        .value("CAMERA_DEVICE", MEDIACAP_REQUIRES_CAMERA_DEVICE)
        .value("AUDIO_INPUT", MEDIACAP_REQUIRES_AUDIO_INPUT)
        .value("AUDIO_OUTPUT", MEDIACAP_REQUIRES_AUDIO_OUTPUT)
        .value("PIPEWIRE", MEDIACAP_REQUIRES_PIPEWIRE)
        .value("SCREEN_PERMISSION", MEDIACAP_REQUIRES_SCREEN_PERMISSION)
        .value("CAMERA_PERMISSION", MEDIACAP_REQUIRES_CAMERA_PERMISSION)
        .value("MICROPHONE_PERMISSION", MEDIACAP_REQUIRES_MICROPHONE_PERMISSION)
        .value("SYSTEM_AUDIO_PERMISSION", MEDIACAP_REQUIRES_SYSTEM_AUDIO_PERMISSION)
        .value("PROCESS_ID", MEDIACAP_REQUIRES_PROCESS_ID)
        .value("FLEXAUDIO", MEDIACAP_REQUIRES_FLEXAUDIO);
    py::enum_<MediaCap_SourceType>(module, "SourceType")
        .value("UNKNOWN", MEDIACAP_SOURCE_TYPE_UNKNOWN)
        .value("MONITOR", MEDIACAP_SOURCE_TYPE_MONITOR)
        .value("WINDOW", MEDIACAP_SOURCE_TYPE_WINDOW)
        .value("CAMERA", MEDIACAP_SOURCE_TYPE_CAMERA)
        .value("AUDIO_INPUT", MEDIACAP_SOURCE_TYPE_AUDIO_INPUT)
        .value("AUDIO_OUTPUT", MEDIACAP_SOURCE_TYPE_AUDIO_OUTPUT)
        .value("AUDIO_PROCESS", MEDIACAP_SOURCE_TYPE_AUDIO_PROCESS)
        .value("AUDIO_MIX", MEDIACAP_SOURCE_TYPE_AUDIO_MIX);

    py::enum_<MediaCap_ErrorCode>(module, "ErrorCode")
        .value("NONE", MEDIACAP_ERROR_NONE)
        .value("INVALID_ARGUMENT", MEDIACAP_ERROR_INVALID_ARGUMENT)
        .value("DEVICE_UNAVAILABLE", MEDIACAP_ERROR_DEVICE_UNAVAILABLE)
        .value("DEVICE_LOST", MEDIACAP_ERROR_DEVICE_LOST)
        .value("TIMEOUT", MEDIACAP_ERROR_TIMEOUT)
        .value("BUFFER_OVERFLOW", MEDIACAP_ERROR_BUFFER_OVERFLOW)
        .value("CAPTURE_FAILED", MEDIACAP_ERROR_CAPTURE_FAILED)
        .value("ENCODING_FAILED", MEDIACAP_ERROR_ENCODING_FAILED)
        .value("IO", MEDIACAP_ERROR_IO)
        .value("INTERNAL", MEDIACAP_ERROR_INTERNAL)
        .value("UNSUPPORTED", MEDIACAP_ERROR_UNSUPPORTED)
        .value("PERMISSION_DENIED", MEDIACAP_ERROR_PERMISSION_DENIED);

    py::enum_<MediaCap_ErrorComponent>(module, "ErrorComponent")
        .value("API", MEDIACAP_ERROR_COMPONENT_API)
        .value("DISCOVERY", MEDIACAP_ERROR_COMPONENT_DISCOVERY)
        .value("SESSION", MEDIACAP_ERROR_COMPONENT_SESSION)
        .value("CAPTURE", MEDIACAP_ERROR_COMPONENT_CAPTURE)
        .value("MIXER", MEDIACAP_ERROR_COMPONENT_MIXER)
        .value("ENCODER", MEDIACAP_ERROR_COMPONENT_ENCODER);

    py::enum_<MediaCap_LogLevel>(module, "LogLevel")
        .value("NONE", MEDIACAP_LOG_LEVEL_NONE)
        .value("ERROR", MEDIACAP_LOG_LEVEL_ERROR)
        .value("WARNING", MEDIACAP_LOG_LEVEL_WARNING)
        .value("INFO", MEDIACAP_LOG_LEVEL_INFO)
        .value("DEBUG", MEDIACAP_LOG_LEVEL_DEBUG)
        .value("VERBOSE", MEDIACAP_LOG_LEVEL_VERBOSE);

    py::enum_<MediaCap_PixelFormat>(module, "PixelFormat")
        .value("UNKNOWN", MEDIACAP_PIXEL_FORMAT_UNKNOWN)
        .value("BGRA", MEDIACAP_PIXEL_FORMAT_BGRA)
        .value("BGR24", MEDIACAP_PIXEL_FORMAT_BGR24)
        .value("RGBA", MEDIACAP_PIXEL_FORMAT_RGBA)
        .value("RGB24", MEDIACAP_PIXEL_FORMAT_RGB24)
        .value("NV12", MEDIACAP_PIXEL_FORMAT_NV12)
        .value("I420", MEDIACAP_PIXEL_FORMAT_I420);

    py::enum_<MediaCap_FrameOrientation>(module, "FrameOrientation")
        .value("TOP_DOWN", MEDIACAP_ORIENTATION_TOP_DOWN)
        .value("BOTTOM_UP", MEDIACAP_ORIENTATION_BOTTOM_UP);

    py::enum_<MediaCap_AudioDeviceType>(module, "AudioDeviceType")
        .value("UNKNOWN", MEDIACAP_AUDIO_DEVICE_TYPE_UNKNOWN)
        .value("INPUT", MEDIACAP_AUDIO_DEVICE_TYPE_INPUT)
        .value("OUTPUT", MEDIACAP_AUDIO_DEVICE_TYPE_OUTPUT)
        .value("LOOPBACK", MEDIACAP_AUDIO_DEVICE_TYPE_LOOPBACK);

    py::enum_<MediaCap_SampleFormat>(module, "SampleFormat")
        .value("UNKNOWN", MEDIACAP_SAMPLE_FORMAT_UNKNOWN)
        .value("U8", MEDIACAP_SAMPLE_FORMAT_U8)
        .value("S16", MEDIACAP_SAMPLE_FORMAT_S16)
        .value("S32", MEDIACAP_SAMPLE_FORMAT_S32)
        .value("FLT", MEDIACAP_SAMPLE_FORMAT_FLT)
        .value("DBL", MEDIACAP_SAMPLE_FORMAT_DBL)
        .value("U8P", MEDIACAP_SAMPLE_FORMAT_U8P)
        .value("S16P", MEDIACAP_SAMPLE_FORMAT_S16P)
        .value("S32P", MEDIACAP_SAMPLE_FORMAT_S32P)
        .value("FLTP", MEDIACAP_SAMPLE_FORMAT_FLTP)
        .value("DBLP", MEDIACAP_SAMPLE_FORMAT_DBLP);

    py::enum_<MediaCap_ContainerFormat>(module, "ContainerFormat")
        .value("DEFAULT", MEDIACAP_CONTAINER_DEFAULT)
        .value("MP4", MEDIACAP_CONTAINER_MP4)
        .value("MKV", MEDIACAP_CONTAINER_MKV)
        .value("WEBM", MEDIACAP_CONTAINER_WEBM)
        .value("M4A", MEDIACAP_CONTAINER_M4A)
        .value("FLAC", MEDIACAP_CONTAINER_FLAC)
        .value("MP3", MEDIACAP_CONTAINER_MP3)
        .value("WAV", MEDIACAP_CONTAINER_WAV)
        .value("OPUS", MEDIACAP_CONTAINER_OPUS);

    py::enum_<MediaCap_VideoCodec>(module, "VideoCodec")
        .value("DEFAULT", MEDIACAP_VIDEO_CODEC_DEFAULT)
        .value("H264", MEDIACAP_VIDEO_CODEC_H264)
        .value("VP9", MEDIACAP_VIDEO_CODEC_VP9)
        .value("AV1", MEDIACAP_VIDEO_CODEC_AV1);

    py::enum_<MediaCap_AudioCodec>(module, "AudioCodec")
        .value("DEFAULT", MEDIACAP_AUDIO_CODEC_DEFAULT)
        .value("AAC", MEDIACAP_AUDIO_CODEC_AAC)
        .value("OPUS", MEDIACAP_AUDIO_CODEC_OPUS)
        .value("VORBIS", MEDIACAP_AUDIO_CODEC_VORBIS)
        .value("FLAC", MEDIACAP_AUDIO_CODEC_FLAC)
        .value("MP3", MEDIACAP_AUDIO_CODEC_MP3)
        .value("PCM", MEDIACAP_AUDIO_CODEC_PCM);

    py::enum_<MediaCap_AudioCaptureKind>(module, "AudioCaptureKind")
        .value("MIC", MEDIACAP_AUDIO_CAPTURE_MIC)
        .value("SYSTEM", MEDIACAP_AUDIO_CAPTURE_SYSTEM)
        .value("PROCESS", MEDIACAP_AUDIO_CAPTURE_PROCESS)
        .value("MIX", MEDIACAP_AUDIO_CAPTURE_MIX);

    py::enum_<MediaCap_ProcessAudioMode>(module, "ProcessAudioMode")
        .value("INCLUDE", MEDIACAP_PROCESS_INCLUDE)
        .value("EXCLUDE", MEDIACAP_PROCESS_EXCLUDE);

    py::enum_<MediaCap_AudioAvailability>(module, "AudioAvailability")
        .value("UNAVAILABLE", MEDIACAP_AUDIO_UNAVAILABLE)
        .value("AVAILABLE", MEDIACAP_AUDIO_AVAILABLE)
        .value("AVAILABILITY_UNKNOWN", MEDIACAP_AUDIO_AVAILABILITY_UNKNOWN);

    py::enum_<MediaCap_AudioPidScope>(module, "AudioPidScope")
        .value("NONE", MEDIACAP_PID_SCOPE_NONE)
        .value("EXACT", MEDIACAP_PID_SCOPE_EXACT)
        .value("TREE", MEDIACAP_PID_SCOPE_TREE);
}
}
