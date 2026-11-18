from pathlib import Path
import shutil
import subprocess
import sys

from SCons.Script import Action


def add_install_target(env, library, cli, ffmpeg_lib, flex_archive, is_windows, python_files, python_dir, gui):
    def install(target, source, env):
        prefix = Path(env['prefix']).resolve()
        (prefix / 'lib').mkdir(parents=True, exist_ok=True)
        (prefix / 'bin').mkdir(parents=True, exist_ok=True)
        shutil.copytree('include', prefix / 'include', dirs_exist_ok=True)
        for node in library:
            file = Path(node.get_abspath())
            if file.suffix not in ('.exp', '.pdb', '.ilk'):
                destination = 'bin' if file.suffix == '.dll' else 'lib'
                shutil.copy2(file, prefix / destination / file.name)
        for node in gui:
            shutil.copy2(node.get_abspath(), prefix / 'bin')
        if python_files:
            package = prefix / python_dir
            package.mkdir(parents=True, exist_ok=True)
            for node in python_files:
                shutil.copy2(node.get_abspath(), package)
        if cli:
            shutil.copy2(cli[0].get_abspath(), prefix / 'bin')
            notices = prefix / 'share/licenses/MediaCapCLI'
            notices.mkdir(parents=True, exist_ok=True)
            shutil.copy2('cli/third_party/CLI11/LICENSE', notices / 'CLI11-LICENSE.txt')
            shutil.copy2('cli/third_party/nlohmann/LICENSE', notices / 'nlohmann-json-LICENSE.txt')
        if flex_archive:
            flex_notices = prefix / 'share/licenses/MediaCap/flexaudio'
            flex_notices.mkdir(parents=True, exist_ok=True)
            for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
                shutil.copy2(Path('src/third_party/flexaudio') / name, flex_notices / name)
        screen_notices = prefix / 'share/licenses/MediaCap/screen_capture_lite'
        screen_notices.mkdir(parents=True, exist_ok=True)
        shutil.copy2('src/third_party/screen_capture_lite/LICENSE', screen_notices / 'LICENSE')
        if not is_windows:
            notices = prefix / 'share/licenses/MediaCap'
            notices.mkdir(parents=True, exist_ok=True)
            for name in ('LICENSE', 'COPYING'):
                shutil.copy2(name, notices / name)
            return 0
        command = [sys.executable, 'scripts/win/install_metadata.py', '--prefix', str(prefix),
                   '--dependency-dir', ffmpeg_lib, '--shared', str(bool(env['shared'])).lower(),
                   '--debug', str(bool(env['debug'])).lower()]
        if flex_archive and not env['shared']:
            command += ['--rust-archive', flex_archive[0].get_abspath()]
        return subprocess.call(command, env=env['ENV'])

    target = env.Alias('install', library + (cli or []) + python_files + gui, Action(install))
    env.AlwaysBuild(target)
