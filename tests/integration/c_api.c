#include <MediaCap_c.h>

int exercise_c_api(void) {
    MediaCap_FeatureRequirements requirements = {0};
    if (!MediaCap_GetFeatureRequirements(MEDIACAP_FEATURE_SCREEN, &requirements) ||
        !requirements.included_in_build || !(requirements.prerequisites & MEDIACAP_REQUIRES_DISPLAY_SESSION) ||
        MediaCap_GetFeatureRequirements(MEDIACAP_FEATURE_SCREEN, NULL)) return 0;
    MediaCap_AudioCapabilities audio = {0};
    if (!MediaCap_GetAudioCapabilities(&audio) ||
        !MediaCap_GetFeatureRequirements(MEDIACAP_FEATURE_PROCESS_AUDIO, &requirements) ||
        requirements.included_in_build != audio.backend_enabled ||
        !(requirements.prerequisites & MEDIACAP_REQUIRES_FLEXAUDIO)) return 0;
    MediaCap_Session_Handle session = MediaCap_Session_Create();
    if (!session) return 0;

    MediaCap_Error error = {0};
    int passed = MediaCap_Session_GetId(session) != 0 &&
        MediaCap_Session_SetFrameCallbacks(session, NULL, NULL, NULL) &&
        !MediaCap_Session_StartCapture(session) &&
        !MediaCap_Session_IsCapturing(session) &&
        MediaCap_Session_GetError(session, &error) &&
        error.code == MEDIACAP_ERROR_INVALID_ARGUMENT;

    MediaCap_Session_StopCapture(session);
    MediaCap_Session_Destroy(session);
    MediaCap_VideoEncoderConfig video = {0};
    MediaCap_RecordingConfig config = {"bad\xFF" ".mkv", MEDIACAP_CONTAINER_DEFAULT, &video, NULL};
    passed = passed && !MediaCap_ValidateRecordingFormat(&config) &&
        MediaCap_GetLastError(&error) && error.code == MEDIACAP_ERROR_INVALID_ARGUMENT;
    config.output_filename = "test.wav";
    passed = passed && !MediaCap_ValidateRecordingFormat(&config) &&
        MediaCap_GetLastError(&error) && error.code == MEDIACAP_ERROR_UNSUPPORTED;
    return passed && !MediaCap_CaptureMonitorScreenshot(NULL, NULL, NULL, NULL) &&
        !MediaCap_CaptureWindowScreenshot(NULL, NULL, NULL, NULL);
}
