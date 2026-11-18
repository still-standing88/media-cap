import platform
from pathlib import Path

from SCons.Errors import UserError


def find_archive(directory, name, is_windows):
    extension = '.lib' if is_windows else '.a'
    for stem in (name, 'lib' + name, name + '-static', 'lib' + name + '-static'):
        candidate = directory / (stem + extension)
        if candidate.is_file():
            return candidate
    raise UserError(f'Missing FFmpeg dependency {name} in {directory}')


def configure_ffmpeg(env, is_windows, is_macos):
    triplet = 'explicit FFmpeg directories'
    if env['use_vcpkg']:
        if not env['vcpkg_root']:
            raise UserError('Set vcpkg_root or VCPKG_ROOT, or select use_vcpkg=0')
        machine = platform.machine().lower()
        arch = 'arm64' if machine in ('arm64', 'aarch64') else 'x64' if machine in ('x86_64', 'amd64') else 'x86'
        suffix = 'windows-static' if is_windows else 'osx' if is_macos else 'linux'
        triplet = env['vcpkg_triplet'] or f'{arch}-{suffix}'
        if is_windows and not triplet.endswith('-windows-static'):
            raise UserError('MediaCap requires a *-windows-static triplet')
        installed = Path(env['vcpkg_root']) / 'installed' / triplet
        ffmpeg_include = installed / 'include'
        ffmpeg_lib = installed / ('debug/lib' if env['debug'] else 'lib')
    else:
        include = env['ffmpeg_include_dir']
        lib = (env['ffmpeg_debug_lib_dir'] if env['debug'] else '') or env['ffmpeg_lib_dir']
        if not include or not lib:
            raise UserError('Set ffmpeg_include_dir and ffmpeg_lib_dir when use_vcpkg=0')
        ffmpeg_include = Path(include)
        ffmpeg_lib = Path(lib)
    if not ffmpeg_include.is_dir():
        raise UserError(f'Missing FFmpeg headers directory: {ffmpeg_include}')

    env.Append(CPPPATH=[str(ffmpeg_include.resolve())])
    names = ['avformat', 'avcodec', 'swscale', 'swresample', 'avutil', 'aom',
             'dav1d', 'openh264', 'opus', 'vorbisenc', 'vorbis', 'ogg', 'vpx', 'mp3lame']
    if is_windows:
        names.append('mpghip')
    libraries = [env.File(str(find_archive(ffmpeg_lib, name, is_windows).resolve())) for name in names]
    return triplet, str(ffmpeg_lib.resolve()), libraries


def verify_ffmpeg(env, output):
    # Fail closed using the same linked-library policy probe as CMake and Meson.
    policy_source = Path('scripts/ffmpeg_check.cpp').read_text(encoding='utf-8').replace(
        '../src/integrations/FFmpegPolicy.h', 'integrations/FFmpegPolicy.h')
    policy = env.Configure(conf_dir=output + 'ffmpeg-policy')
    policy_ok, policy_output = policy.TryRun(policy_source, '.cpp')
    env = policy.Finish()
    if not policy_ok:
        raise UserError('FFmpeg LGPL/encoder policy failed: ' + policy_output)
    print(policy_output)
    return env
