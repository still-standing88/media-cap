set(src "${PROJECT_SOURCE_DIR}/src")
add_executable(mediacap_unit_tests
    timeline.cpp audio_mixer.cpp screenshot.cpp flexaudio_adapter.cpp
    "${src}/audio/AudioMixer.cpp"
    "${src}/capture/Screenshot.cpp"
    "${src}/capture/FlexAudioCapturer.cpp"
    "${src}/utils/ErrorReporter.cpp")
mediacap_internal_test(mediacap_unit_tests)
target_compile_definitions(mediacap_unit_tests PRIVATE MEDIACAP_HAS_FLEXAUDIO=1)
add_test(NAME unit COMMAND mediacap_unit_tests)
set_tests_properties(unit PROPERTIES LABELS unit TIMEOUT 30)
