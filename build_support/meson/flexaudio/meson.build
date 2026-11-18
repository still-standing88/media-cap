flexaudio_deps = []
flexaudio_enabled = false
if not get_option('flexaudio').disabled()
  flex_cargo = find_program(get_option('flexaudio_cargo'), required: get_option('flexaudio'))
  if flex_cargo.found()
    if meson.is_cross_build() and get_option('flexaudio_target') == ''
      error('Set flexaudio_target to the matching Rust target triple for cross compilation')
    endif
    flex_python = find_program('python3', 'python')
    flex_script = files('../../../scripts/build_flexaudio.py')
    flex_manifest = meson.project_source_root() / 'src/integrations/flexaudio_bridge/Cargo.toml'
    flex_inputs = run_command(flex_python, flex_script, '--manifest', flex_manifest, '--sources', check: true).stdout().strip().split('\n')
    flex_args = []
    if is_windows and get_option('b_vscrt').startswith('static')
      flex_args += '--crt-static'
    endif
    if get_option('flexaudio_offline')
      flex_args += '--offline'
    endif
    flex_archive_path = meson.current_build_dir() / (is_windows ? 'mediacap_flexaudio.lib' : 'libmediacap_flexaudio.a')
    flex_lib = custom_target('mediacap_flexaudio',
      output: is_windows ? 'mediacap_flexaudio.lib' : 'libmediacap_flexaudio.a',
      command: [flex_python, flex_script, '--cargo', flex_cargo.full_path(), '--manifest', flex_manifest,
        '--target-dir', meson.project_build_root() / 'flexaudio-target', '--target', get_option('flexaudio_target'),
        '--output', '@OUTPUT@'] + flex_args,
      depend_files: files(flex_inputs), install: get_option('default_library') == 'static', install_dir: get_option('libdir'))
    flex_native = []
    if is_windows
      foreach lib : ['kernel32', 'ntdll', 'userenv', 'ws2_32', 'dbghelp']
        flex_native += cpp.find_library(lib)
      endforeach
    elif is_linux
      flex_native += [dependency('alsa'), dependency('libpipewire-0.3'), cpp.find_library('dl'),
        cpp.find_library('m'), cpp.find_library('rt'), cpp.find_library('util')]
    elif is_macos
      flex_native += [dependency('appleframeworks', modules: ['CoreAudio', 'AudioToolbox', 'CoreFoundation', 'Foundation', 'AVFoundation', 'Security']),
        cpp.find_library('objc'), cpp.find_library('iconv')]
    endif
    flexaudio_deps += declare_dependency(link_with: flex_lib, dependencies: flex_native)
    flexaudio_enabled = true
    install_data('../../../src/third_party/flexaudio/LICENSE', '../../../src/third_party/flexaudio/THIRD_PARTY_NOTICES.md',
      install_dir: get_option('datadir') / 'licenses/MediaCap/flexaudio')
  endif
endif
