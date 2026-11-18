cli_test_python = find_program('python3', 'python')
cli_fixture = executable('MediaCap_cli_fixture', cli_sources + files('fake_api.cpp'),
  include_directories: [cli11, include_directories('../../../include')],
  dependencies: platform_deps,
  cpp_args: platform_compile_args + ['-DMEDIACAP_STATIC'])
test('integration-cli-contract', cli_test_python,
  args: [files('test_cli.py'), cli], suite: 'integration', timeout: 120)
test('integration-cli-discovery', cli_test_python,
  args: [files('test_discovery.py'), cli_fixture], suite: 'integration', timeout: 60)
test('integration-cli-capture', cli_test_python,
  args: [files('test_capture.py'), cli_fixture], suite: 'integration', timeout: 60)
