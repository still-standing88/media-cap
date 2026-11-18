set(SCL_COMMON_SRC
    "third_party/screen_capture_lite/src_cpp/SCCommon.cpp"
    "third_party/screen_capture_lite/src_cpp/ScreenCapture.cpp"
    "third_party/screen_capture_lite/src_cpp/ScreenCapture.c"
    "third_party/screen_capture_lite/src_cpp/ThreadManager.cpp"
)

set(SCL_LINK_LIBS "")
set(SCL_PLATFORM_INCLUDE_DIR "")

if(WIN32)
    set(SCL_PLATFORM_SRC
        "third_party/screen_capture_lite/src_cpp/windows/GetWindows.cpp"
        "third_party/screen_capture_lite/src_cpp/windows/GetMonitors.cpp"
        "third_party/screen_capture_lite/src_cpp/windows/DXFrameProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/windows/GDIFrameProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/windows/GDIMouseProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/windows/ThreadRunner.cpp"
    )
    # oleaut32 (VariantInit/VariantClear) is required by the ccap DirectShow
    # device-enumeration path in ccap_imp_windows.cpp (ProviderDirectShow::
    # enumerateDevices reads moniker friendly names via IPropertyBag + VARIANT).
    # Not needed by the old vendored ccap snapshot; required after syncing to
    # current upstream ccap - see rules.md's CameraCapture vendoring note.
    set(SCL_LINK_LIBS
        dxgi
        d3d11
        dwmapi
        gdi32
        ws2_32
        secur32
        mfuuid
        mf
        mfplat
        mfreadwrite
        strmiids
        ole32
        oleaut32
        user32
        bcrypt
        kernel32
        advapi32
        shell32
        winmm
    )
    set(SCL_PLATFORM_INCLUDE_DIR "third_party/screen_capture_lite/include/windows")

elseif(APPLE)
    set(SCL_PLATFORM_SRC
        "third_party/screen_capture_lite/src_cpp/ios/GetWindows.cpp"
        "third_party/screen_capture_lite/src_cpp/ios/NSMouseCapture.m"
        "third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.mm"
        "third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/ios/NSMouseProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/ios/CGFrameProcessor.cpp"
        "third_party/screen_capture_lite/src_cpp/ios/GetMonitors.cpp"
        "third_party/screen_capture_lite/src_cpp/ios/ThreadRunner.cpp"
    )
    set(SCL_LINK_LIBS
        "-framework CoreFoundation"
        "-framework Cocoa"
        "-framework CoreMedia"
        "-framework AVFoundation"
        "-framework CoreGraphics"
        "-framework CoreVideo"
        "-framework ImageIO"
        "-framework Security"
        "-framework SystemConfiguration"
    )
    set(SCL_PLATFORM_INCLUDE_DIR "third_party/screen_capture_lite/include/ios")

else()
    find_package(X11 REQUIRED)
    if(NOT X11_Xext_FOUND OR NOT X11_Xtst_FOUND OR NOT X11_Xfixes_FOUND OR NOT X11_Xinerama_FOUND)
        message(FATAL_ERROR "Xext, XTest, Xfixes and Xinerama are required for X11 screen capture.")
    endif()
    set(SCL_PLATFORM_SRC
       "third_party/screen_capture_lite/src_cpp/linux/X11MouseProcessor.cpp"
       "third_party/screen_capture_lite/src_cpp/linux/X11FrameProcessor.cpp"
       "third_party/screen_capture_lite/src_cpp/linux/GetMonitors.cpp"
       "third_party/screen_capture_lite/src_cpp/linux/GetWindows.cpp"
       "third_party/screen_capture_lite/src_cpp/linux/ThreadRunner.cpp"
    )
    set(SCL_LINK_LIBS
        X11::X11 X11::Xext X11::Xfixes X11::Xtst X11::Xinerama
        ${CMAKE_DL_LIBS}
        m
    )
    set(SCL_PLATFORM_INCLUDE_DIR "third_party/screen_capture_lite/include/linux")
endif()
