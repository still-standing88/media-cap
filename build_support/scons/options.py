import os
import sys

from SCons.Script import BoolVariable, Environment, Help, PathVariable, Variables


def create_environment(arguments):
    options = Variables(None, arguments)
    options.Add(BoolVariable('shared', 'Build shared MediaCap; 0 builds static', True))
    options.Add(BoolVariable('debug', 'Build Debug; 0 builds Release', False))
    options.Add(BoolVariable('cli', 'Build the MediaCap command-line executable', False))
    options.Add(BoolVariable('gui', 'Build the AudioPix desktop application', False))
    options.Add('slint_root', 'External Slint C++ SDK prefix for AudioPix', '')
    options.Add(BoolVariable('python', 'Build the MediaCap CPython extension', False))
    options.Add('python_executable', 'Interpreter for the CPython extension', sys.executable)
    options.Add(BoolVariable('examples', 'Build the standalone examples', True))
    options.Add(BoolVariable('tests', 'Build and run unit and integration tests', False))
    options.Add('build_dir', 'Optional directory for this build configuration', '')
    options.Add(PathVariable('prefix', 'Installation prefix', 'dist/scons', PathVariable.PathAccept))
    options.Add(PathVariable('vcpkg_root', 'vcpkg root', os.environ.get('VCPKG_ROOT', ''), PathVariable.PathAccept))
    options.Add('vcpkg_triplet', 'vcpkg triplet; defaults to the host architecture', '')
    options.Add(BoolVariable('use_vcpkg', 'Resolve FFmpeg from the selected vcpkg triplet', True))
    options.Add('ffmpeg_include_dir', 'FFmpeg headers when use_vcpkg=0', '')
    options.Add('ffmpeg_lib_dir', 'FFmpeg archives when use_vcpkg=0', '')
    options.Add('ffmpeg_debug_lib_dir', 'Optional Debug FFmpeg archive directory when use_vcpkg=0', '')
    options.Add(BoolVariable('flexaudio', 'Build the vendored FlexAudio backend', True))
    options.Add('flexaudio_cargo', 'Cargo executable', 'cargo')
    options.Add('flexaudio_target', 'Rust target triple; empty for native', '')
    options.Add(BoolVariable('flexaudio_offline', 'Require cached Cargo dependencies', False))
    env = Environment(variables=options, ENV=os.environ)
    Help(options.GenerateHelpText(env))
    return env
