set(MEDIACAP_SOURCES
    core/Session.cpp
    interface/MediaCap_c.cpp
    interface/FlexAudioDiscovery.cpp
    discovery/Discovery.cpp
    capture/AudioCapturer.cpp
    capture/CameraCapturer.cpp
    capture/FlexAudioCapturer.cpp
    capture/ScreenCapturer.cpp
    capture/Screenshot.cpp
    audio/AudioMixer.cpp
    integrations/FFmpegMuxer.cpp
    utils/ErrorReporter.cpp
)

set(MINIAUDIO_SRC "third_party/miniaudio/miniaudio.c")

include("${PROJECT_SOURCE_DIR}/cmake/ScreenCapture.cmake")

include("${PROJECT_SOURCE_DIR}/cmake/CameraCapture.cmake")

add_library(MediaCap
    ${MEDIACAP_SOURCES}
    ${MINIAUDIO_SRC}
    ${SCL_COMMON_SRC}
    ${SCL_PLATFORM_SRC}
    ${CCAP_COMMON_SRC}
    ${CCAP_PLATFORM_SRC}
    ${CCAP_AVX2_SRC}
)

set_target_properties(MediaCap PROPERTIES CXX_STANDARD 17)
target_compile_features(MediaCap PUBLIC cxx_std_17)
target_compile_definitions(MediaCap PRIVATE ${CCAP_COMPILE_DEFINITIONS})
if(NOT MSVC AND BUILD_SHARED_LIBS)
    set_target_properties(MediaCap PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endif()

if(BUILD_SHARED_LIBS)
    target_compile_definitions(MediaCap PRIVATE MEDIACAP_EXPORT)
else()
    target_compile_definitions(MediaCap PUBLIC MEDIACAP_STATIC)
endif()

target_include_directories(MediaCap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(MediaCap PRIVATE
    "."
    "third_party/screen_capture_lite/include"
    "${SCL_PLATFORM_INCLUDE_DIR}"
    "third_party/CameraCapture/include"
    "third_party/miniaudio"
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(MediaCap PRIVATE 
    ${FFMPEG_LIBS}
    Threads::Threads
    ${SCL_LINK_LIBS}
    ${CCAP_LINK_LIBS}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(MediaCap PUBLIC atomic)
    if(BUILD_SHARED_LIBS)
        # Bind private archive symbols locally for the FFmpeg assembly relocations.
        target_link_options(MediaCap PRIVATE "LINKER:--exclude-libs,ALL")
    endif()
endif()

include("${PROJECT_SOURCE_DIR}/cmake/CompilerOptions.cmake")

add_library(MediaCap::MediaCap ALIAS MediaCap)
include("${PROJECT_SOURCE_DIR}/cmake/FlexAudio.cmake")

install(FILES "third_party/screen_capture_lite/LICENSE"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/MediaCap/screen_capture_lite)
install(TARGETS MediaCap
    EXPORT MediaCapTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(MSVC AND BUILD_SHARED_LIBS)
    install(FILES "$<TARGET_PDB_FILE:MediaCap>" DESTINATION ${CMAKE_INSTALL_BINDIR}
        CONFIGURATIONS Debug OPTIONAL)
endif()
