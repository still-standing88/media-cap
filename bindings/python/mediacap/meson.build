binding_link_args = []
binding_cpp_args = ['-I' + binding_include, '-DNOMINMAX']
if cpp.get_argument_syntax() == 'msvc'
  binding_cpp_args += ['/bigobj']
endif
binding_install_rpath = ''
if get_option('default_library') == 'shared' and not is_windows
  # The package and SDK library are installed under the same prefix.
  binding_loader = is_macos ? '@loader_path' : '$ORIGIN'
  binding_rel_libdir = run_command(binding_python, '-c',
    'import os, sys; print(os.path.relpath(sys.argv[1], sys.argv[2]))',
    get_option('prefix') / get_option('libdir'),
    binding_python.get_install_dir(subdir: 'mediacap'), check: true).stdout().strip()
  binding_install_rpath = binding_loader / binding_rel_libdir
elif is_linux and get_option('default_library') == 'static'
  binding_link_args = ['-Wl,--exclude-libs,ALL']
endif

binding_extension = binding_python.extension_module('_native', binding_sources,
  dependencies: [binding_python_dep, mediacap_dep],
  cpp_args: binding_cpp_args,
  link_args: binding_link_args,
  install_rpath: binding_install_rpath,
  subdir: 'mediacap',
  install: true
)
foreach package_file : binding_package_files
  configure_file(input: package_file, output: package_file, copy: true)
endforeach
binding_python.install_sources(binding_package_files, subdir: 'mediacap', pure: false)
binding_runtime = []
if is_windows and get_option('default_library') == 'shared'
  binding_runtime += [custom_target('mediacap-python-dll',
    input: mediacap_lib,
    output: 'MediaCap.dll',
    command: [binding_python, files('../../../build_support/copy_runtime.py'), '@INPUT@', '@OUTPUT@'],
    build_by_default: true,
    install: true,
    install_dir: binding_python.get_install_dir(subdir: 'mediacap'))]
endif
if get_option('build_tests')
  test('python-bindings', binding_python,
    args: files('../../../tests/integration/python_bindings.py'),
    env: {'PYTHONPATH': meson.current_build_dir() / '..'},
    depends: [binding_extension] + binding_runtime)
endif
