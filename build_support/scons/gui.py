from pathlib import Path
import subprocess
import sys

from SCons.Errors import UserError
from SCons.Script import Action
from targets import objects

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from slint_sdk import locate


def build_gui(consumer, output, is_windows, is_macos):
    system = 'windows' if is_windows else 'darwin' if is_macos else 'linux'
    try:
        compiler, include, link, runtime = locate(consumer['slint_root'], system, consumer['ENV'])
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise UserError(str(error)) from error

    env = consumer.Clone()
    directory = output + 'gui/'
    env.Append(CPPPATH=[directory, str(include)], CPPDEFINES=['WIN32_LEAN_AND_MEAN'])
    env.Append(LIBS=[env.File(str(link))])
    if is_windows:
        env.Append(CXXFLAGS=['/std:c++20', '/bigobj'])
        env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS', '/ENTRY:mainCRTStartup'])
    else:
        env.Append(CXXFLAGS=['-std=c++20'], RPATH=[str(runtime.parent)])
        if is_macos:
            env.AppendUnique(FRAMEWORKS=['ApplicationServices', 'Carbon', 'CoreGraphics', 'IOKit'])
            env.Append(LINKFLAGS=['-Wl,-rpath,@loader_path/../lib'])
        else:
            env.ParseConfig('pkg-config --cflags --libs xkbcommon')
            env.Append(LINKFLAGS=[r'-Wl,-rpath,\$$ORIGIN/../lib'])

    def generate(target, source, env):
        return subprocess.call([source[1].get_abspath(), '-f', 'cpp', '--style', 'fluent',
                                '--embed-resources', 'embed-files', '--cpp-file', target[1].get_abspath(),
                                '-o', target[0].get_abspath(), source[0].get_abspath()], env=env['ENV'])

    header, source = env.Command([directory + 'audiopix.h', directory + 'audiopix_ui.cpp'],
                                 ['gui/ui/audiopix.slint', str(compiler)],
                                 Action(generate, 'Compiling AudioPix Slint interface'))
    compiled = objects(env, ['gui/src/main.cpp', 'gui/src/settings.cpp', 'gui/src/capture.cpp'], output)
    generated = env.Clone()
    if is_windows:
        generated.Append(CCFLAGS=['/W0'])
    else:
        generated.Append(CCFLAGS=['-w'])
    compiled.append(generated.Object(directory + 'audiopix_ui', source))
    env.Depends(compiled, header)
    gui = env.Program(output + 'AudioPix', compiled)
    if is_windows:
        gui += env.InstallAs(output + 'slint_cpp.dll', str(runtime))
    return gui
