set(src "${PROJECT_SOURCE_DIR}/src")
set(screen "${src}/third_party/screen_capture_lite")
add_executable(mediacap_pipeline_tests session_pipeline.cpp screen_backend.cpp codecs.cpp
    "${src}/audio/AudioMixer.cpp"
    "${src}/integrations/FFmpegMuxer.cpp"
    "${src}/utils/ErrorReporter.cpp"
    "${screen}/src_cpp/ScreenCapture.cpp"
    "${screen}/src_cpp/SCCommon.cpp"
    "${screen}/src_cpp/ThreadManager.cpp")
mediacap_internal_test(mediacap_pipeline_tests)
target_include_directories(mediacap_pipeline_tests PRIVATE "${screen}/include")
add_test(NAME integration-pipeline COMMAND mediacap_pipeline_tests)

add_executable(mediacap_api_tests public_api.cpp c_api.c)
target_compile_definitions(mediacap_api_tests PRIVATE MEDIACAP_EXPECT_FLEXAUDIO=$<BOOL:${MEDIACAP_FLEXAUDIO}>)
target_link_libraries(mediacap_api_tests PRIVATE mediacap_test_main MediaCap::MediaCap)
add_test(NAME integration-api COMMAND mediacap_api_tests)
set_tests_properties(integration-pipeline integration-api PROPERTIES LABELS integration TIMEOUT 60)

if(MEDIACAP_FLEXAUDIO AND NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(flex_test_flags)
    if(WIN32)
        list(APPEND flex_test_flags --crt-static)
    endif()
    if(MEDIACAP_FLEXAUDIO_OFFLINE)
        list(APPEND flex_test_flags --offline)
    endif()
    add_test(NAME integration-flexaudio-bridge COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/scripts/build_flexaudio.py" --cargo "${FLEXAUDIO_CARGO}"
        --manifest "${PROJECT_SOURCE_DIR}/src/integrations/flexaudio_bridge/Cargo.toml"
        --target "${MEDIACAP_FLEXAUDIO_TARGET}"
        --target-dir "${PROJECT_BINARY_DIR}/src/flexaudio-target" --test ${flex_test_flags})
    set_tests_properties(integration-flexaudio-bridge PROPERTIES LABELS integration TIMEOUT 180)
endif()
