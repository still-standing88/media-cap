use_vcpkg = get_option('use_vcpkg')
vcpkg_triplet = get_option('vcpkg_triplet')
ffmpeg_include_dir = get_option('ffmpeg_include_dir')
ffmpeg_lib_dir = get_option('ffmpeg_lib_dir')


if use_vcpkg and vcpkg_triplet == ''
  if is_windows
    arch = host_machine.cpu_family()
    if arch == 'x86_64'
      vcpkg_triplet = 'x64-windows-static'
    elif arch == 'aarch64'
      vcpkg_triplet = 'arm64-windows-static'
    else
      vcpkg_triplet = 'x86-windows-static'
    endif
  elif is_macos
    arch = host_machine.cpu_family()
    if arch == 'aarch64'
      vcpkg_triplet = 'arm64-osx'
    else
      vcpkg_triplet = 'x64-osx'
    endif
  elif is_linux
    arch = host_machine.cpu_family()
    if arch == 'x86_64'
      vcpkg_triplet = 'x64-linux'
    elif arch == 'aarch64'
      vcpkg_triplet = 'arm64-linux'
    else
      vcpkg_triplet = 'x86-linux'
    endif
  endif
  message('Auto-detected vcpkg triplet: ' + vcpkg_triplet)
endif

# Resolve exactly the requested installation; PATH/pkg-config must not override vcpkg.
ffmpeg_deps = []
ffmpeg_include_dirs = []
if is_windows and cpp.get_argument_syntax() == 'msvc' and get_option('b_vscrt') != 'static_from_buildtype'
  error('MediaCap requires -Db_vscrt=static_from_buildtype (/MT or /MTd)')
endif
if is_windows and cpp.get_argument_syntax() == 'msvc' and get_option('buildtype') in ['plain', 'custom']
  error('Use debug, debugoptimized, release or minsize so static CRT selection is explicit')
endif
if use_vcpkg
  vcpkg_root = get_option('vcpkg_root')
  if vcpkg_root == ''
    python_env = find_program('python3', 'python')
    vcpkg_root = run_command(python_env, '-c', 'import os; print(os.environ.get("VCPKG_ROOT", ""))', check: true).stdout().strip()
  endif
  if vcpkg_root == ''
    error('Set vcpkg_root or VCPKG_ROOT')
  endif
  if is_windows and not vcpkg_triplet.endswith('-windows-static')
    error('Windows dependencies must use a static CRT triplet (*-windows-static)')
  endif
  vcpkg_installed = vcpkg_root / 'installed' / vcpkg_triplet
  ffmpeg_include_dir = vcpkg_installed / 'include'
  ffmpeg_lib_dir = vcpkg_installed / (get_option('buildtype') == 'debug' ? 'debug/lib' : 'lib')
elif get_option('buildtype') == 'debug' and get_option('ffmpeg_debug_lib_dir') != ''
  ffmpeg_lib_dir = get_option('ffmpeg_debug_lib_dir')
endif
if ffmpeg_include_dir == '' or ffmpeg_lib_dir == ''
  error('Specify FFmpeg include and library directories')
endif
# Meson requires source-relative paths for dependencies cached inside the checkout.
# External paths on another Windows drive remain absolute.
ffmpeg_include_dirs = [fs.relative_to(ffmpeg_include_dir, meson.project_source_root())]
foreach name : ['avformat', 'avcodec', 'swscale', 'swresample', 'avutil', 'aom', 'dav1d', 'openh264', 'opus', 'vorbisenc', 'vorbis', 'ogg', 'vpx']
  ffmpeg_deps += cpp.find_library(name, dirs: [ffmpeg_lib_dir], required: true, static: true)
endforeach
mp3lame_lib = cpp.find_library('mp3lame', dirs: [ffmpeg_lib_dir], required: false, static: true)
if not mp3lame_lib.found()
  mp3lame_lib = cpp.find_library('mp3lame-static', dirs: [ffmpeg_lib_dir], required: true, static: true)
endif
ffmpeg_deps += mp3lame_lib
if is_windows
  # Upstream LAME's separate decoder archive is needed by the encoder archive.
  ffmpeg_deps += cpp.find_library('mpghip-static', dirs: [ffmpeg_lib_dir], required: true, static: true)
endif
if is_windows
  foreach name : ['bcrypt', 'mfuuid', 'strmiids', 'ole32', 'oleaut32', 'uuid', 'user32', 'advapi32', 'ws2_32', 'secur32', 'mfplat', 'mf']
    ffmpeg_deps += cpp.find_library(name)
  endforeach
elif is_macos
  ffmpeg_deps += [dependency('appleframeworks', modules: ['AppKit', 'AudioToolbox',
    'AVFoundation', 'CoreFoundation', 'CoreImage', 'CoreMedia', 'CoreVideo',
    'VideoToolbox', 'Security', 'CoreServices']), cpp.find_library('iconv')]
elif is_linux
  ffmpeg_deps += [dependency('threads'), cpp.find_library('dl'), cpp.find_library('m')]
endif
if not meson.can_run_host_binaries()
  error('A host executable wrapper is required to validate FFmpeg license/codec policy')
endif
probe_include_dir = fs.relative_to(ffmpeg_include_dir, meson.current_source_dir())
codec_result = cpp.run(files('../../../scripts/ffmpeg_check.cpp'), dependencies: ffmpeg_deps,
  include_directories: include_directories(probe_include_dir), name: 'LGPL FFmpeg and all required encoders')
if not codec_result.compiled() or codec_result.returncode() != 0
  error('FFmpeg policy probe failed: ' + codec_result.stdout() + codec_result.stderr())
endif
message(codec_result.stdout())
message('FFmpeg configuration complete')
