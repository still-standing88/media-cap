project('MediaCap', ['c', 'cpp'],
  version : '1.0.0',
  meson_version : '>=1.3.0',
  default_options : [
    'cpp_std=c++17',
    'c_std=c11',
    'warning_level=3',
    'werror=false',
    'b_vscrt=static_from_buildtype'
  ])

cpp = meson.get_compiler('cpp')
c = meson.get_compiler('c')
fs = import('fs')

is_windows = host_machine.system() == 'windows'
is_macos = host_machine.system() == 'darwin'
is_linux = host_machine.system() == 'linux'

if get_option('default_library') == 'both'
  error('Select one library type with --default-library=shared or static')
endif
if is_macos
  add_languages('objc', 'objcpp', native: false)
endif

subdir('build_support/meson/ffmpeg')
subdir('build_support/meson/platform')
subdir('build_support/meson/sources')
subdir('build_support/meson/flexaudio')

inc_dirs_paths = [
  'include',
  'src',
  'src/third_party/screen_capture_lite/include',
  'src/third_party/CameraCapture/include',
  'src/third_party/miniaudio'
]

if is_windows
  inc_dirs_paths += ['src/third_party/screen_capture_lite/include/windows']
elif is_macos
  inc_dirs_paths += ['src/third_party/screen_capture_lite/include/ios']
elif is_linux
  inc_dirs_paths += ['src/third_party/screen_capture_lite/include/linux']
endif

if ffmpeg_include_dirs.length() > 0
  inc_dirs_paths += ffmpeg_include_dirs
endif

inc_dirs = include_directories(inc_dirs_paths)

compile_defs = ['-DMEDIACAP_HAS_FLEXAUDIO=' + (flexaudio_enabled ? '1' : '0')]
if get_option('default_library') == 'shared'
  compile_defs += ['-DMEDIACAP_EXPORT']
else
  compile_defs += ['-DMEDIACAP_STATIC']
endif

# Windows consumers and all frontends use MediaCap.dll. ELF/Mach-O retain
# versioned shared-library names; a Meson version would suffix the Windows DLL.
library_version = {}
if not is_windows
  library_version = {'version': meson.project_version()}
endif
if get_option('default_library') == 'static' and get_option('build_python')
  library_version += {'pic': true}
endif
mediacap_lib = library('MediaCap',
  mediacap_sources,
  include_directories: inc_dirs,
  link_with: internal_deps,
  dependencies: ffmpeg_deps + platform_deps + [thread_dep] + flexaudio_deps,
  cpp_args: platform_compile_args + platform_cpp_only_args + compile_defs + ccap_compile_args,
  c_args: platform_compile_args + compile_defs,
  objc_args: is_macos ? platform_compile_args + compile_defs : [],
  objcpp_args: is_macos ? platform_compile_args + compile_defs + ccap_compile_args + ['-std=c++17'] : [],
  link_args: platform_link_args + (is_linux and get_option('default_library') == 'shared' ? ['-Wl,--exclude-libs,ALL'] : []),
  install: true,
  kwargs: library_version
)

mediacap_dep = declare_dependency(
  link_with: mediacap_lib,
  include_directories: include_directories('include'),
  dependencies: ffmpeg_deps + platform_deps + [thread_dep] + flexaudio_deps,
  compile_args: get_option('default_library') == 'static' ? ['-DMEDIACAP_STATIC'] : []
)

install_subdir('include', install_dir: get_option('includedir'), strip_directory: true)

if get_option('build_examples')
  subdir('examples')
endif
if get_option('build_tests')
  subdir('tests')
endif
if get_option('build_cli')
  subdir('cli')
  if get_option('build_tests')
    subdir('tests/integration/cli')
  endif
endif

if get_option('build_python')
  subdir('bindings/python')
endif
if get_option('build_gui')
  subdir('gui')
endif
subdir('build_support/meson/packaging')
