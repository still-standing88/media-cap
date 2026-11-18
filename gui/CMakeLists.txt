find_package(Slint 1.17 CONFIG QUIET)
if(NOT TARGET Slint::Slint)
    if(NOT MEDIACAP_FETCH_SLINT)
        message(FATAL_ERROR "AudioPix requires Slint C++ 1.17 or newer. Set Slint_DIR or CMAKE_PREFIX_PATH.")
    endif()
    include(FetchContent)
    FetchContent_Declare(Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR api/cpp)
    FetchContent_MakeAvailable(Slint)
endif()

add_executable(AudioPix src/main.cpp src/settings.cpp src/capture.cpp)
target_compile_features(AudioPix PRIVATE cxx_std_20)
target_compile_definitions(AudioPix PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(AudioPix PRIVATE MediaCap::MediaCap Slint::Slint Threads::Threads)
slint_target_sources(AudioPix ui/audiopix.slint)

if(WIN32)
    target_link_libraries(AudioPix PRIVATE user32)
    if(MSVC)
        target_compile_options(AudioPix PRIVATE /utf-8)
        target_link_options(AudioPix PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    endif()
    # Official Windows SDKs store the Slint runtime under lib.
    if(TARGET slint_cpp-shared)
        add_custom_command(TARGET AudioPix POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:slint_cpp-shared>" "$<TARGET_FILE_DIR:AudioPix>"
            VERBATIM)
        install(FILES "$<TARGET_FILE:slint_cpp-shared>" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
elseif(APPLE)
    foreach(framework ApplicationServices Carbon CoreGraphics IOKit)
        find_library(audiopix_${framework} ${framework} REQUIRED)
        target_link_libraries(AudioPix PRIVATE "${audiopix_${framework}}")
    endforeach()
    set_target_properties(AudioPix PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}"
        INSTALL_RPATH_USE_LINK_PATH ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
    target_link_libraries(AudioPix PRIVATE PkgConfig::XKBCOMMON)
    set_target_properties(AudioPix PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
        INSTALL_RPATH_USE_LINK_PATH ON)
endif()

install(TARGETS AudioPix RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
