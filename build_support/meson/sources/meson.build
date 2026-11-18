mediacap_sources = files(
  '../../../src/core/Session.cpp',
  '../../../src/interface/MediaCap_c.cpp',
  '../../../src/discovery/Discovery.cpp',
  '../../../src/utils/ErrorReporter.cpp',
  '../../../src/interface/FlexAudioDiscovery.cpp',
  '../../../src/capture/FlexAudioCapturer.cpp',
  '../../../src/capture/AudioCapturer.cpp',
  '../../../src/capture/CameraCapturer.cpp',
  '../../../src/capture/ScreenCapturer.cpp',
  '../../../src/capture/Screenshot.cpp',
  '../../../src/audio/AudioMixer.cpp',
  '../../../src/integrations/FFmpegMuxer.cpp',
  '../../../src/third_party/miniaudio/miniaudio.c'
)


scl_sources = files(
  '../../../src/third_party/screen_capture_lite/src_cpp/SCCommon.cpp',
  '../../../src/third_party/screen_capture_lite/src_cpp/ScreenCapture.c',
  '../../../src/third_party/screen_capture_lite/src_cpp/ScreenCapture.cpp',
  '../../../src/third_party/screen_capture_lite/src_cpp/ThreadManager.cpp'
)

if is_windows
  scl_sources += files(
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/DXFrameProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/GDIFrameProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/GDIMouseProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/GetMonitors.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/GetWindows.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/windows/ThreadRunner.cpp'
  )
elif is_macos
  scl_sources += files(
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/CGFrameProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/GetMonitors.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/GetWindows.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.mm',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/NSMouseCapture.m',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/NSMouseProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/ios/ThreadRunner.cpp'
  )
elif is_linux
  scl_sources += files(
    '../../../src/third_party/screen_capture_lite/src_cpp/linux/GetMonitors.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/linux/GetWindows.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/linux/ThreadRunner.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/linux/X11FrameProcessor.cpp',
    '../../../src/third_party/screen_capture_lite/src_cpp/linux/X11MouseProcessor.cpp'
  )
endif

mediacap_sources += scl_sources

ccap_compile_args = []
if is_windows or is_macos or is_linux
  ccap_sources = files(
    '../../../src/third_party/CameraCapture/src/ccap_core.cpp',
    '../../../src/third_party/CameraCapture/src/ccap_convert.cpp',
    '../../../src/third_party/CameraCapture/src/ccap_convert_frame.cpp',
    '../../../src/third_party/CameraCapture/src/ccap_convert_neon.cpp',
    '../../../src/third_party/CameraCapture/src/ccap_imp.cpp',
    '../../../src/third_party/CameraCapture/src/ccap_utils.cpp'
  )
  # ccap_convert_neon.cpp is required on every platform above (not just ARM):
  # ccap_convert.cpp unconditionally calls ccap::hasNEON(), which is only
  # defined in that TU. On non-ARM64 targets ENABLE_NEON_IMP compiles it down
  # to a trivial "return false" stub, so this adds no real NEON dependency.

  if is_windows
    ccap_sources += files(
      '../../../src/third_party/CameraCapture/src/ccap_imp_windows.cpp',
      '../../../src/third_party/CameraCapture/src/ccap_imp_windows_msmf.cpp'
    )
    # ccap_imp_windows_msmf.cpp (MSMF backend) is required alongside the
    # DirectShow backend: ccap_core.cpp unconditionally declares and calls
    # createProviderMSMF() (for backend auto-detection/fallback), so omitting
    # this TU is a link error, not just a missing feature.
  elif is_macos
    ccap_sources += files(
      '../../../src/third_party/CameraCapture/src/ccap_convert_apple.cpp'
    )
    ccap_apple = static_library('ccap_apple',
      '../../../src/third_party/CameraCapture/src/ccap_imp_apple.mm',
      pic: get_option('build_python') or get_option('b_staticpic'),
      objcpp_args: platform_compile_args + ['-fobjc-arc', '-std=c++17'],
      include_directories: include_directories('../../../src/third_party/CameraCapture/include'),
      dependencies: platform_deps)
    internal_deps += [ccap_apple]
  elif is_linux
    ccap_sources += files('../../../src/third_party/CameraCapture/src/ccap_imp_linux.cpp')
  endif

  ccap_avx2_src = files('../../../src/third_party/CameraCapture/src/ccap_convert_avx2.cpp')
  avx2_flag = cpp.get_argument_syntax() == 'msvc' ? '/arch:AVX2' : '-mavx2'
  if cpp.has_argument(avx2_flag)

    avx2_lib = static_library('ccap_avx2',
      ccap_avx2_src,
      pic: get_option('build_python') or get_option('b_staticpic'),
      cpp_args: platform_compile_args + [avx2_flag],
      include_directories: include_directories('../../../src/third_party/CameraCapture/include')
    )
    internal_deps += [avx2_lib]
  else
    ccap_compile_args += '-DENABLE_AVX2_IMP=0'
    ccap_sources += ccap_avx2_src
  endif

  mediacap_sources += ccap_sources
endif
