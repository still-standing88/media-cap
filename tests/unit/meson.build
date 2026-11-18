unit_tests = executable('mediacap_unit_tests',
  'timeline.cpp', 'audio_mixer.cpp', 'screenshot.cpp', 'flexaudio_adapter.cpp',
  '../../src/audio/AudioMixer.cpp', '../../src/capture/Screenshot.cpp',
  '../../src/capture/FlexAudioCapturer.cpp', '../../src/utils/ErrorReporter.cpp',
  include_directories: inc_dirs, dependencies: test_internal_deps,
  cpp_args: test_internal_args + ['-DMEDIACAP_HAS_FLEXAUDIO=1'],
  link_args: platform_link_args)
test('unit', unit_tests, suite: 'unit', timeout: 30)
