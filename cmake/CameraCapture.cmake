set(CCAP_COMMON_SRC
    "third_party/CameraCapture/src/ccap_core.cpp"
    "third_party/CameraCapture/src/ccap_convert.cpp"
    "third_party/CameraCapture/src/ccap_convert_frame.cpp"
    "third_party/CameraCapture/src/ccap_convert_neon.cpp"
    "third_party/CameraCapture/src/ccap_imp.cpp"
    "third_party/CameraCapture/src/ccap_utils.cpp"
)
# ccap_convert_neon.cpp is required on every platform below (not just ARM):
# ccap_convert.cpp unconditionally calls ccap::hasNEON(), which is only
# defined in that TU. On non-ARM64 targets ENABLE_NEON_IMP compiles it down
# to a trivial "return false" stub, so this adds no real NEON dependency.

include(CheckCXXCompilerFlag)
if(MSVC)
    set(CCAP_AVX2_FLAG "/arch:AVX2")
else()
    set(CCAP_AVX2_FLAG "-mavx2")
endif()
check_cxx_compiler_flag("${CCAP_AVX2_FLAG}" CCAP_SUPPORTS_AVX2)
set(CCAP_AVX2_SRC "third_party/CameraCapture/src/ccap_convert_avx2.cpp")
if(CCAP_SUPPORTS_AVX2)
    set_source_files_properties(${CCAP_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "${CCAP_AVX2_FLAG}")
else()
    set(CCAP_COMPILE_DEFINITIONS ENABLE_AVX2_IMP=0)
endif()

set(CCAP_LINK_LIBS "")

if(WIN32)
    set(CCAP_PLATFORM_SRC
        "third_party/CameraCapture/src/ccap_imp_windows.cpp"
        "third_party/CameraCapture/src/ccap_imp_windows_msmf.cpp"
    )
    # ccap_imp_windows_msmf.cpp (MSMF backend) is required alongside the
    # DirectShow backend: ccap_core.cpp unconditionally declares and calls
    # createProviderMSMF() (for backend auto-detection/fallback), so omitting
    # this TU is a link error, not just a missing feature.

elseif(APPLE)
    set(CCAP_PLATFORM_SRC
        "third_party/CameraCapture/src/ccap_imp_apple.mm"
        "third_party/CameraCapture/src/ccap_convert_apple.cpp"
    )
    set_source_files_properties("third_party/CameraCapture/src/ccap_imp_apple.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set(CCAP_LINK_LIBS
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate"
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CCAP_PLATFORM_SRC "third_party/CameraCapture/src/ccap_imp_linux.cpp")
endif()
