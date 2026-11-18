foreach example_name : fs.read('targets.txt').strip().split('\n')
  example_name = example_name.strip()
  example_exe = executable(example_name, example_name + '.cpp',
    dependencies: mediacap_dep, cpp_args: platform_compile_args, link_args: platform_link_args)
  if get_option('build_tests')
    test('example-' + example_name, example_exe, args: ['--help'], suite: 'integration', timeout: 15)
  endif
endforeach

if is_windows and get_option('default_library') == 'shared'
  example_python = find_program('python3', 'python')
  custom_target('example_runtime', input: mediacap_lib, output: 'MediaCap.dll',
    command: [example_python, files('../build_support/copy_runtime.py'), '@INPUT@', '@OUTPUT@'],
    build_by_default: true)
endif
