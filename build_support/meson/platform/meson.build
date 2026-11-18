thread_dep = dependency('threads')

platform_deps = []
internal_deps = []
platform_compile_args = []
platform_link_args = []
if is_windows and cpp.get_argument_syntax() == 'msvc' and get_option('buildtype') == 'debug'
  platform_link_args += ['/NODEFAULTLIB:libcmt', '/INCREMENTAL:NO']
endif

if is_windows
  # oleaut32 (VariantInit/VariantClear) is required by the ccap DirectShow
  # device-enumeration path in ccap_imp_windows.cpp (ProviderDirectShow::
  # enumerateDevices reads moniker friendly names via IPropertyBag + VARIANT).
  platform_deps += [
    cpp.find_library('dxgi'),
    cpp.find_library('d3d11'),
    cpp.find_library('dwmapi'),
    cpp.find_library('gdi32'),
    cpp.find_library('ws2_32'),
    cpp.find_library('secur32'),
    cpp.find_library('mfuuid'),
    cpp.find_library('mf'),
    cpp.find_library('mfplat'),
    cpp.find_library('mfreadwrite'),
    cpp.find_library('strmiids'),
    cpp.find_library('ole32'),
    cpp.find_library('oleaut32'),
    cpp.find_library('user32'),
    cpp.find_library('bcrypt'),
    cpp.find_library('kernel32'),
    cpp.find_library('advapi32'),
    cpp.find_library('shell32'),
    cpp.find_library('winmm')
  ]

  platform_compile_args += [
    '-DNOMINMAX',
    '-D_CRT_SECURE_NO_WARNINGS'
  ]

  if cpp.get_id() == 'msvc'
    platform_compile_args += [
      '/MP',
      '/Zc:__cplusplus',
      '/Zc:preprocessor',
      '/permissive-',     # standards-conforming parsing (matches gcc/clang default behavior)
      '/utf-8',            # source *and* execution charset - several vendored files (ccap) contain UTF-8 comments
      '/EHsc',              # standard C++ exception model; be explicit rather than relying on the default
      '/W4',
      '/wd4100',            # unreferenced formal parameter - extremely noisy across the vendored capture backends
      '/wd4127',            # conditional expression is constant - common in template/macro-heavy vendored code
    ]

    # Release-only codegen flags. Note this deliberately does NOT add /GL
    # (whole-program optimization) - an earlier version of this comment claimed
    # it did, but /GL was never in the list. Adding it by hand would also need
    # /LTCG at link time and would fight meson's own bookkeeping; if you want
    # LTO here, use meson's built-in `-Db_lto=true` rather than open-coding it,
    # for the same reason CRT selection uses b_vscrt above.
    if get_option('buildtype') == 'release'
      platform_compile_args += ['/Oi', '/Gy']
      platform_link_args += ['/OPT:REF', '/OPT:ICF']
    endif
  endif

elif is_macos
  platform_deps += [
    dependency('appleframeworks', modules: [
      'CoreFoundation',
      'Cocoa',
      'CoreMedia',
      'AVFoundation',
      'CoreGraphics',
      'CoreVideo',
      'ImageIO',
      'Security',
      'SystemConfiguration',
      'Foundation',
      'Accelerate'
    ])
  ]

  platform_compile_args += [
    '-Wno-deprecated-declarations', # AVFoundation/Carbon capture APIs deprecate faster than we can chase them
  ]

  if get_option('default_library') == 'shared'
    platform_compile_args += ['-fvisibility=hidden', '-fvisibility-inlines-hidden']
  endif

elif is_linux
  x11_dep = dependency('x11', required: true)
  xext_dep = dependency('xext', required: true)
  xtest_dep = dependency('xtst', required: true)
  xfixes_dep = dependency('xfixes', required: true)
  xinerama_dep = dependency('xinerama', required: true)

  platform_deps += [x11_dep, xext_dep, xtest_dep, xfixes_dep, xinerama_dep]
  platform_deps += [
    cpp.find_library('pthread'),
    cpp.find_library('dl'),
    cpp.find_library('m'),
    cpp.find_library('atomic')
  ]

  platform_compile_args += [
    '-D_FILE_OFFSET_BITS=64', # capture output files can exceed 2GiB
  ]

  if get_option('default_library') == 'shared'
    platform_compile_args += ['-fvisibility=hidden', '-fvisibility-inlines-hidden']
  endif
endif

# Cross-platform hardening for gcc/clang (MSVC has its own defaults/flags above).
# -D_FORTIFY_SOURCE requires optimization > 0, so only add it for non-debug buildtypes.
if (is_linux or is_macos) and cpp.get_id() != 'msvc'
  # Only flags valid for both the C and C++ frontends - this list feeds both
  # cpp_args and c_args below. C++-only warnings (e.g. -Wnon-virtual-dtor)
  # deliberately live in platform_cpp_only_args instead.
  platform_compile_args += cpp.get_supported_arguments([
    '-fstack-protector-strong',
    '-Wshadow',
  ])
  if get_option('buildtype') != 'debug' and get_option('buildtype') != 'plain'
    platform_compile_args += cpp.get_supported_arguments(['-D_FORTIFY_SOURCE=2'])
  endif
endif

platform_cpp_only_args = []
if (is_linux or is_macos) and cpp.get_id() != 'msvc'
  platform_cpp_only_args += cpp.get_supported_arguments(['-Wnon-virtual-dtor'])
endif
