audiopix_python = find_program('python3', 'python')
slint_sdk = run_command(audiopix_python, files('../build_support/slint_sdk.py'),
  get_option('slint_root'), host_machine.system(), check: true).stdout().strip().split('\n')
slint_compiler = find_program(slint_sdk[0])
audiopix_ui = custom_target('audiopix_ui',
  input: 'ui/audiopix.slint', output: ['audiopix.h', 'audiopix_ui.cpp'],
  command: [slint_compiler, '-f', 'cpp', '--style', 'fluent', '--embed-resources', 'embed-files',
    '--cpp-file', '@OUTPUT1@', '-o', meson.current_build_dir() / 'audiopix.h', '@INPUT@'])

audiopix_deps = [mediacap_dep]
audiopix_args = platform_compile_args + ['-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN', '-I' + slint_sdk[1]]
audiopix_link_args = platform_link_args + [slint_sdk[2]]
audiopix_rpath = ''
if is_windows
  audiopix_args += ['/bigobj', '/utf-8']
  audiopix_link_args += ['/SUBSYSTEM:WINDOWS', '/ENTRY:mainCRTStartup']
  custom_target('audiopix_slint_runtime', input: slint_sdk[3], output: 'slint_cpp.dll',
    command: [audiopix_python, files('../build_support/copy_runtime.py'), '@INPUT@', '@OUTPUT@'],
    install: true, install_dir: get_option('bindir'), build_by_default: true)
  if get_option('default_library') == 'shared'
    custom_target('audiopix_mediacap_runtime', input: mediacap_lib, output: 'MediaCap.dll',
      command: [audiopix_python, files('../build_support/copy_runtime.py'), '@INPUT@', '@OUTPUT@'],
      build_by_default: true)
  endif
elif is_macos
  audiopix_deps += dependency('appleframeworks', modules: ['ApplicationServices', 'Carbon', 'CoreGraphics', 'IOKit'])
elif is_linux
  audiopix_deps += dependency('xkbcommon')
endif
if not is_windows
  relative_libdir = fs.relative_to(get_option('prefix') / get_option('libdir'),
    get_option('prefix') / get_option('bindir'))
  audiopix_rpath = (is_macos ? '@loader_path' : '$ORIGIN') / relative_libdir
  audiopix_rpath += ':' + fs.parent(slint_sdk[3])
endif

executable('AudioPix', ['src/main.cpp', 'src/settings.cpp', 'src/capture.cpp'], audiopix_ui,
  dependencies: audiopix_deps,
  cpp_args: audiopix_args, link_args: audiopix_link_args,
  override_options: ['cpp_std=c++20', 'warning_level=0'], install: true,
  build_rpath: is_windows ? '' : fs.parent(slint_sdk[3]), install_rpath: audiopix_rpath)
