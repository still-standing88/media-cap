cli_sources = files(fs.read('sources.txt').strip().split('\n'))
cli11 = include_directories('third_party/CLI11', 'third_party/nlohmann', is_system: true)
cli_install_rpath = ''
if not is_windows and get_option('default_library') == 'shared'
  relative_libdir = fs.relative_to(get_option('prefix') / get_option('libdir'),
    get_option('prefix') / get_option('bindir'))
  cli_install_rpath = (is_macos ? '@loader_path' : '$ORIGIN') / relative_libdir
endif
cli = executable('MediaCap', cli_sources,
  dependencies: mediacap_dep, include_directories: cli11,
  cpp_args: platform_compile_args, link_args: platform_link_args,
  install: true, install_rpath: cli_install_rpath)

if is_windows and get_option('default_library') == 'shared'
  cli_runtime_python = find_program('python3', 'python')
  custom_target('cli_runtime', input: mediacap_lib, output: 'MediaCap.dll',
    command: [cli_runtime_python, files('../build_support/copy_runtime.py'), '@INPUT@', '@OUTPUT@'],
    build_by_default: true)
endif

install_data('third_party/CLI11/LICENSE', rename: 'CLI11-LICENSE.txt',
  install_dir: get_option('datadir') / 'licenses/MediaCapCLI')
install_data('third_party/nlohmann/LICENSE', rename: 'nlohmann-json-LICENSE.txt',
  install_dir: get_option('datadir') / 'licenses/MediaCapCLI')
