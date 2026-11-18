import platform


def configure_platform(env, is_windows, is_macos, is_linux):
    env.Append(CPPPATH=[
        'include',
        'src',
        'src/third_party/screen_capture_lite/include',
        'src/third_party/CameraCapture/include',
        'src/third_party/miniaudio',
    ])

    env.Append(CPPDEFINES=['MEDIACAP_EXPORT' if env['shared'] else 'MEDIACAP_STATIC'])

    if env['debug']:
        env.Append(CPPDEFINES=['_DEBUG'])
    else:
        env.Append(CPPDEFINES=['NDEBUG'])

    sources = [
        'src/core/Session.cpp',
        'src/interface/MediaCap_c.cpp',
        'src/discovery/Discovery.cpp',
        'src/utils/ErrorReporter.cpp',
        'src/interface/FlexAudioDiscovery.cpp',
        'src/capture/FlexAudioCapturer.cpp',
        'src/capture/AudioCapturer.cpp',
        'src/capture/CameraCapturer.cpp',
        'src/capture/ScreenCapturer.cpp',
        'src/capture/Screenshot.cpp',
        'src/audio/AudioMixer.cpp',
        'src/integrations/FFmpegMuxer.cpp',
        'src/third_party/miniaudio/miniaudio.c',
        'src/third_party/screen_capture_lite/src_cpp/SCCommon.cpp',
        'src/third_party/screen_capture_lite/src_cpp/ScreenCapture.c',
        'src/third_party/screen_capture_lite/src_cpp/ScreenCapture.cpp',
        'src/third_party/screen_capture_lite/src_cpp/ThreadManager.cpp',
        'src/third_party/CameraCapture/src/ccap_core.cpp',
        'src/third_party/CameraCapture/src/ccap_convert.cpp',
        'src/third_party/CameraCapture/src/ccap_convert_frame.cpp',
        'src/third_party/CameraCapture/src/ccap_convert_neon.cpp',
        'src/third_party/CameraCapture/src/ccap_imp.cpp',
        'src/third_party/CameraCapture/src/ccap_utils.cpp',
    ]

    avx2_flag = None
    avx2_source = None

    if is_windows:
        env.Append(CPPPATH=['src/third_party/screen_capture_lite/include/windows'])
        sources += [
            'src/third_party/screen_capture_lite/src_cpp/windows/DXFrameProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/windows/GDIFrameProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/windows/GDIMouseProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/windows/GetMonitors.cpp',
            'src/third_party/screen_capture_lite/src_cpp/windows/GetWindows.cpp',
            'src/third_party/screen_capture_lite/src_cpp/windows/ThreadRunner.cpp',
            'src/third_party/CameraCapture/src/ccap_imp_windows.cpp',
            # ccap_core.cpp unconditionally calls createProviderMSMF() for backend
            # auto-detection/fallback - a hard link dependency, not opt-in.
            'src/third_party/CameraCapture/src/ccap_imp_windows_msmf.cpp',
        ]
        if 'arm' not in platform.machine().lower():
            avx2_flag = '/arch:AVX2'
            avx2_source = 'src/third_party/CameraCapture/src/ccap_convert_avx2.cpp'

        env.Append(CPPDEFINES=['NOMINMAX', '_CRT_SECURE_NO_WARNINGS'])
        env.Append(LIBS=[
            'dxgi', 'd3d11', 'dwmapi', 'gdi32', 'ws2_32', 'secur32', 'mfuuid', 'mf',
            'mfplat', 'mfreadwrite', 'strmiids', 'ole32', 'oleaut32', 'user32',
            'bcrypt', 'kernel32', 'advapi32', 'shell32', 'winmm', 'uuid',
        ])

        if env['CC'] == 'cl' or env.Detect('cl'):
            env.Append(CCFLAGS=['/MP', '/Zc:__cplusplus', '/Zc:preprocessor', '/EHsc', '/utf-8', '/permissive-', '/W4', '/wd4100'])
            # Match the CRT in the library and every executable that links to it.
            env.Append(CCFLAGS=['/MTd' if env['debug'] else '/MT'])
            if not env['debug']:
                env.Append(CCFLAGS=['/O2'])
            else:
                # Embed debug info per object so parallel compiles do not contend
                # over MSVC's default vc140.pdb in the source directory.
                env.Append(CCFLAGS=['/Z7', '/Od'])
                env.Append(LINKFLAGS=['/DEBUG', '/NODEFAULTLIB:libcmt'])

    elif is_macos:
        env.Append(CPPPATH=['src/third_party/screen_capture_lite/include/ios'])
        sources += [
            'src/third_party/screen_capture_lite/src_cpp/ios/CGFrameProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/ios/GetMonitors.cpp',
            'src/third_party/screen_capture_lite/src_cpp/ios/GetWindows.cpp',
            'src/third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.mm',
            'src/third_party/screen_capture_lite/src_cpp/ios/NSFrameProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/ios/NSMouseCapture.m',
            'src/third_party/screen_capture_lite/src_cpp/ios/NSMouseProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/ios/ThreadRunner.cpp',
            'src/third_party/CameraCapture/src/ccap_convert_apple.cpp',
        ]
        if platform.machine().lower() in ('x86_64', 'amd64'):
            avx2_flag = '-mavx2'
            avx2_source = 'src/third_party/CameraCapture/src/ccap_convert_avx2.cpp'
        env.Append(FRAMEWORKS=[
            'CoreFoundation', 'Cocoa', 'CoreMedia', 'AVFoundation', 'CoreGraphics',
            'CoreVideo', 'Security', 'SystemConfiguration', 'Foundation', 'Accelerate', 'ImageIO',
            'AudioToolbox', 'CoreImage', 'VideoToolbox', 'CoreServices',
        ])
        env.Append(LIBS=['iconv'])
        env.Append(CCFLAGS=['-Wall', '-Wextra', '-Wno-deprecated-declarations'])
        env.Append(CCFLAGS=['-g'] if env['debug'] else ['-O2'])
        if env['shared']:
            env.Append(CCFLAGS=['-fvisibility=hidden'])

    elif is_linux:
        env.Append(CPPDEFINES=['_FILE_OFFSET_BITS=64'])
        env.Append(CPPPATH=['src/third_party/screen_capture_lite/include/linux'])
        sources += [
            'src/third_party/screen_capture_lite/src_cpp/linux/GetMonitors.cpp',
            'src/third_party/screen_capture_lite/src_cpp/linux/GetWindows.cpp',
            'src/third_party/screen_capture_lite/src_cpp/linux/ThreadRunner.cpp',
            'src/third_party/screen_capture_lite/src_cpp/linux/X11FrameProcessor.cpp',
            'src/third_party/screen_capture_lite/src_cpp/linux/X11MouseProcessor.cpp',
            'src/third_party/CameraCapture/src/ccap_imp_linux.cpp',
        ]
        if platform.machine().lower() in ('x86_64', 'amd64', 'i386', 'i686'):
            avx2_flag = '-mavx2'
            avx2_source = 'src/third_party/CameraCapture/src/ccap_convert_avx2.cpp'
        env.ParseConfig('pkg-config --cflags --libs x11 xext xtst xfixes xinerama')
        env.Append(LIBS=['pthread', 'dl', 'm', 'atomic'])
        env.Append(CCFLAGS=['-Wall', '-Wextra', '-Wshadow', '-fstack-protector-strong'])
        env.Append(CCFLAGS=['-g'] if env['debug'] else ['-O2', '-D_FORTIFY_SOURCE=2'])
        if env['shared']:
            env.Append(CCFLAGS=['-fvisibility=hidden'])

    env.Append(CFLAGS=['-std=c11'] if not is_windows else [])
    if not is_windows:
        env.Append(CXXFLAGS=['-std=c++17'])
    else:
        env.Append(CXXFLAGS=['/std:c++17'])
    if avx2_source is None:
        env.Append(CPPDEFINES=[('ENABLE_AVX2_IMP', 0)])
        sources.append('src/third_party/CameraCapture/src/ccap_convert_avx2.cpp')
    return sources, avx2_source, avx2_flag
