from pathlib import Path
import subprocess
import sys

from SCons.Script import Action

from targets import objects


def build_tests(env, consumer, output, examples, cli_tests, flex_test):
    test_root = 'tests/'
    doctest = test_root + 'third_party/doctest'
    support = env.Clone()
    support.Append(CPPPATH=[doctest])
    support.Replace(CPPDEFINES=[item for item in env['CPPDEFINES']
        if item not in ('MEDIACAP_EXPORT', 'MEDIACAP_STATIC') and
        not (isinstance(item, tuple) and item[0] == 'MEDIACAP_HAS_FLEXAUDIO')] + ['MEDIACAP_STATIC'])
    main = objects(support, [test_root + 'support/main.cpp'], output)

    unit_env = support.Clone()
    unit_env.Append(CPPDEFINES=[('MEDIACAP_HAS_FLEXAUDIO', 1)])
    unit = unit_env.Program(output + 'mediacap_unit_tests', main + objects(unit_env, [
        'tests/unit/timeline.cpp', 'tests/unit/audio_mixer.cpp', 'tests/unit/screenshot.cpp',
        'tests/unit/flexaudio_adapter.cpp', 'src/audio/AudioMixer.cpp',
        'src/capture/Screenshot.cpp', 'src/capture/FlexAudioCapturer.cpp', 'src/utils/ErrorReporter.cpp',
    ], output + 'unit/'))

    screen = 'src/third_party/screen_capture_lite/src_cpp/'
    pipeline = support.Program(output + 'mediacap_pipeline_tests', main + objects(support, [
        'tests/integration/session_pipeline.cpp', 'tests/integration/screen_backend.cpp',
        'tests/integration/codecs.cpp',
        'src/audio/AudioMixer.cpp', 'src/integrations/FFmpegMuxer.cpp', 'src/utils/ErrorReporter.cpp',
        screen + 'ScreenCapture.cpp', screen + 'SCCommon.cpp', screen + 'ThreadManager.cpp',
    ], output + 'pipeline/'))
    api_env = consumer.Clone()
    api_env.Append(CPPPATH=[doctest], CPPDEFINES=[('MEDIACAP_EXPECT_FLEXAUDIO', int(env['flexaudio']))])
    api = api_env.Program(output + 'mediacap_api_tests', main + objects(api_env, [
        'tests/integration/public_api.cpp', 'tests/integration/c_api.c',
    ], output + 'api/'))

    unit_commands = [[unit[0].get_abspath()]]
    integration_commands = [[node[0].get_abspath()] for node in (pipeline, api)]
    integration_commands.extend([[example.get_abspath(), '--help'] for example in examples])
    integration_commands.extend([[sys.executable, script, node.get_abspath()] for script, node in cli_tests])
    if flex_test:
        integration_commands.append(flex_test)

    def suite(name, inputs, commands):
        def run(target, source, env):
            for command in commands:
                subprocess.run(command, env=env['ENV'], check=True, timeout=180)
            Path(target[0].get_abspath()).write_text('passed\n', encoding='utf-8')
            return 0

        result = env.Command(output + name + '.passed', inputs, Action(run))
        env.AlwaysBuild(result)
        env.Alias(name, result)
        return result

    unit_passed = suite('test-unit', unit, unit_commands)
    integration_passed = suite('test-integration', pipeline + api + examples + [node for _, node in cli_tests],
        integration_commands)
    return unit_passed + integration_passed
