from pathlib import Path
import subprocess
import sys

from SCons.Script import Action


def configure_flexaudio(env, is_windows, is_macos, is_linux, output):
    env.Append(CPPDEFINES=[('MEDIACAP_HAS_FLEXAUDIO', int(env['flexaudio']))])
    if not env['flexaudio']:
        return None, None

    script = str(Path('scripts/build_flexaudio.py').resolve())
    manifest = str(Path('src/integrations/flexaudio_bridge/Cargo.toml').resolve())
    target_dir = str(Path('build-scons/flexaudio-target').resolve())
    sources = subprocess.check_output([sys.executable, script, '--manifest', manifest, '--sources'], text=True).splitlines()
    command = [sys.executable, script, '--cargo', env['flexaudio_cargo'], '--manifest', manifest,
               '--target-dir', target_dir, '--target', env['flexaudio_target']]
    if is_windows:
        command.append('--crt-static')
    if env['flexaudio_offline']:
        command.append('--offline')

    def build(target, source, env):
        return subprocess.call(command + ['--output', target[0].get_abspath()], env=env['ENV'])

    archive_name = 'mediacap_flexaudio.lib' if is_windows else 'libmediacap_flexaudio.a'
    archive = env.Command(output + archive_name, sources + [script], Action(build, varlist=[
        'flexaudio_cargo', 'flexaudio_target', 'flexaudio_offline']))
    env.Append(LIBS=archive)
    if is_windows:
        env.Append(LIBS=['kernel32', 'ntdll', 'userenv', 'ws2_32', 'dbghelp'])
    elif is_macos:
        env.Append(FRAMEWORKS=['CoreAudio', 'AudioToolbox'], LIBS=['objc', 'iconv'])
    elif is_linux:
        env.ParseConfig('pkg-config --cflags --libs alsa libpipewire-0.3')
        env.Append(LIBS=['rt', 'util'])
    return archive, command + ['--test']
