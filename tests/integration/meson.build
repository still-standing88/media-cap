pipeline_tests = executable('mediacap_pipeline_tests',
  'session_pipeline.cpp', 'screen_backend.cpp', 'codecs.cpp',
  '../../src/audio/AudioMixer.cpp', '../../src/integrations/FFmpegMuxer.cpp',
  '../../src/utils/ErrorReporter.cpp',
  '../../src/third_party/screen_capture_lite/src_cpp/ScreenCapture.cpp',
  '../../src/third_party/screen_capture_lite/src_cpp/SCCommon.cpp',
  '../../src/third_party/screen_capture_lite/src_cpp/ThreadManager.cpp',
  include_directories: inc_dirs, dependencies: test_internal_deps,
  cpp_args: test_internal_args, link_args: platform_link_args)
test('integration-pipeline', pipeline_tests, suite: 'integration', timeout: 60)

api_tests = executable('mediacap_api_tests', 'public_api.cpp', 'c_api.c',
  dependencies: [mediacap_dep, test_dep],
  cpp_args: platform_compile_args + ['-DMEDIACAP_EXPECT_FLEXAUDIO=' + (flexaudio_enabled ? '1' : '0')],
  c_args: platform_compile_args, link_args: platform_link_args)
test('integration-api', api_tests, suite: 'integration', timeout: 60)

if flexaudio_enabled and not meson.is_cross_build()
  test('integration-flexaudio-bridge', flex_python, args: [flex_script, '--cargo', flex_cargo.full_path(),
    '--manifest', flex_manifest, '--target', get_option('flexaudio_target'),
    '--target-dir', meson.project_build_root() / 'flexaudio-target',
    '--test'] + flex_args, suite: 'integration', timeout: 180)
endif
