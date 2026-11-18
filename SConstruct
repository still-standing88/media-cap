from pathlib import Path
import sys

sys.path.insert(0, Dir('#build_support/scons').get_abspath())

from capture_platform import configure_platform
from ffmpeg import configure_ffmpeg, verify_ffmpeg
from flexaudio import configure_flexaudio
from gui import build_gui
from install import add_install_target
from options import create_environment
from python_bindings import add_python_test, build_python
from targets import build_consumers, build_library
from test_targets import build_tests

env = create_environment(ARGUMENTS)
is_windows = sys.platform == 'win32'
is_macos = sys.platform == 'darwin'
is_linux = sys.platform.startswith('linux')

mode = 'debug' if env['debug'] else 'release'
linkage = 'shared' if env['shared'] else 'static'
output = Path(env['build_dir'] or f'build-scons/{mode}-{linkage}').as_posix().rstrip('/') + '/'

triplet, ffmpeg_dir, ffmpeg_libraries = configure_ffmpeg(env, is_windows, is_macos)
sources, avx2_source, avx2_flag = configure_platform(env, is_windows, is_macos, is_linux)
env.Prepend(LIBS=ffmpeg_libraries)
env = verify_ffmpeg(env, output)
flex_archive, flex_test = configure_flexaudio(env, is_windows, is_macos, is_linux, output)
if env['python'] and not env['shared'] and not is_windows:
    env.Append(CCFLAGS=['-fPIC'])
print(f'MediaCap build: {mode}, {linkage}, {triplet}')

library = build_library(env, output, sources, avx2_source, avx2_flag, flex_archive)
consumer, examples, cli, cli_tests = build_consumers(env, output, library, is_windows)
python_files, python_dir = (build_python(env, consumer, output, library, is_windows, is_macos)
                            if env['python'] else ([], None))
gui = build_gui(consumer, output, is_windows, is_macos) if env['gui'] else []
Default(library + examples + (cli or []) + python_files + gui)
if env['tests']:
    Default(build_tests(env, consumer, output, examples, cli_tests, flex_test))
    if python_files:
        Default(add_python_test(env, output, python_files))
add_install_target(env, library, cli, ffmpeg_dir, flex_archive, is_windows, python_files, python_dir, gui)
