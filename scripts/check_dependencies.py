"""Check native MediaCap build prerequisites without downloading anything."""

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
FFMPEG_LIBRARIES = (
    "avformat", "avcodec", "swscale", "swresample", "avutil", "aom", "dav1d",
    "openh264", "opus", "vorbisenc", "vorbis", "ogg", "vpx", "mp3lame",
)
FFMPEG_HEADERS = ("libavformat/avformat.h", "libavcodec/avcodec.h", "libswscale/swscale.h",
                  "libswresample/swresample.h", "libavutil/avutil.h")


class DependencyError(RuntimeError):
    pass


@dataclass
class BuildDependencies:
    environment: dict
    programs: dict
    vcpkg_root: Path
    triplet: str
    include_dir: Path
    release_lib_dir: Path
    debug_lib_dir: Path


def add_options(parser):
    parser.add_argument("--frontend", choices=("cmake", "meson", "scons"), default="cmake")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--debug", action="store_const", const="Debug", dest="configuration")
    modes.add_argument("--release", action="store_const", const="Release", dest="configuration")
    variants = parser.add_mutually_exclusive_group()
    variants.add_argument("--shared", action="store_true", dest="shared")
    variants.add_argument("--static", action="store_false", dest="shared")
    parser.add_argument("--cli", action="store_true", help="Build the MediaCap executable")
    parser.add_argument("--tests", action="store_true", help="Build and run library and enabled CLI tests")
    parser.add_argument("--no-examples", action="store_false", dest="examples")
    parser.add_argument("--no-flexaudio", action="store_false", dest="flexaudio")
    parser.add_argument("--offline", action="store_true", help="Use cached FlexAudio Cargo dependencies")
    parser.add_argument("--vcpkg-root", type=Path, help="vcpkg checkout (defaults to VCPKG_ROOT)")
    parser.add_argument("--triplet", help="vcpkg triplet (defaults to the native architecture)")
    parser.add_argument("--ffmpeg-include-dir", type=Path, help="Use explicit FFmpeg headers instead of vcpkg")
    parser.add_argument("--ffmpeg-lib-dir", type=Path, help="Use explicit FFmpeg archives instead of vcpkg")
    parser.add_argument("--ffmpeg-debug-lib-dir", type=Path, help="Explicit Debug archives, if separate")
    parser.set_defaults(configuration="Release", shared=True, examples=True, flexaudio=True)


def platform_settings(options):
    system = platform.system()
    suffixes = {"Windows": "windows-static", "Darwin": "osx", "Linux": "linux"}
    if system not in suffixes:
        raise DependencyError(f"Unsupported build platform: {system}")
    machine = platform.machine().lower()
    architectures = {
        "amd64": "x64", "x86_64": "x64", "arm64": "arm64", "aarch64": "arm64",
        "i386": "x86", "i686": "x86", "x86": "x86",
    }
    arch = architectures.get(machine)
    if options.triplet:
        if not re.fullmatch(r"[A-Za-z0-9_]+(?:-[A-Za-z0-9_]+)+", options.triplet):
            raise DependencyError("Use a valid vcpkg triplet name.")
        triplet = options.triplet
        arch = triplet.split("-", 1)[0]
    elif arch:
        triplet = f"{arch}-{suffixes[system]}"
    else:
        raise DependencyError(f"Pass an explicit vcpkg --triplet for {machine}.")
    if system == "Windows":
        if not triplet.endswith("-windows-static") or arch not in ("x64", "x86", "arm64"):
            raise DependencyError("Windows builds require an x86, x64, or arm64 *-windows-static vcpkg triplet.")
    return system, arch, triplet


def ffmpeg_paths(options, system, triplet, issues):
    explicit = options.ffmpeg_include_dir or options.ffmpeg_lib_dir or options.ffmpeg_debug_lib_dir
    if explicit:
        if options.vcpkg_root:
            raise DependencyError("Choose either --vcpkg-root or explicit FFmpeg directories.")
        if not options.ffmpeg_include_dir or not options.ffmpeg_lib_dir:
            raise DependencyError("Provide both --ffmpeg-include-dir and --ffmpeg-lib-dir.")
        include = options.ffmpeg_include_dir.resolve()
        release = options.ffmpeg_lib_dir.resolve()
        debug = (options.ffmpeg_debug_lib_dir or release).resolve()
        if system == "Windows" and options.configuration == "Debug" and not options.ffmpeg_debug_lib_dir:
            raise DependencyError("Pass --ffmpeg-debug-lib-dir with the /MTd FFmpeg archives for Windows Debug.")
        root = None
    else:
        root = options.vcpkg_root or os.environ.get("VCPKG_ROOT")
        executable = shutil.which("vcpkg")
        if not root and executable:
            root = Path(executable).parent
        if not root:
            raise DependencyError("Set VCPKG_ROOT, pass --vcpkg-root, or provide explicit FFmpeg directories.")
        root = Path(root).resolve()
        installed = root / "installed" / triplet
        include, release, debug = installed / "include", installed / "lib", installed / "debug" / "lib"
        if system != "Windows" and not debug.is_dir() and options.frontend == "cmake":
            debug = release

    missing_headers = [header for header in FFMPEG_HEADERS if not (include / header).is_file()]
    if missing_headers:
        issues.append(f"Missing FFmpeg headers in {include}: {', '.join(missing_headers)}")
    else:
        avutil = include / "libavutil"
        version = {}
        for header in ("version.h", "version_major.h"):
            file = avutil / header
            if file.is_file():
                version.update(re.findall(r"(?m)^\s*#\s*define\s+LIBAVUTIL_VERSION_(MAJOR|MINOR|MICRO)\s+(\d+)",
                                          file.read_text(encoding="utf-8", errors="replace")))
        if len(version) != 3 or tuple(int(version[part]) for part in ("MAJOR", "MINOR", "MICRO")) < (57, 24, 100):
            issues.append(f"FFmpeg 5.1 or newer development headers are required in {include}.")
    windows = system == "Windows"
    names = FFMPEG_LIBRARIES + (("mpghip",) if windows else ())
    extension = ".lib" if windows else ".a"
    selected = debug if options.configuration == "Debug" else release
    directories = dict.fromkeys((release, debug) if options.frontend == "cmake" else (selected,))
    for directory in directories:
        missing = [name for name in names if not any(
            (directory / f"{prefix}{name}{suffix}{extension}").is_file()
            for prefix in ("", "lib") for suffix in ("", "-static"))]
        if missing:
            issues.append(f"Missing FFmpeg or codec archives in {directory}: {', '.join(missing)}")
    if root and (missing_headers or any("Missing FFmpeg or codec archives" in issue for issue in issues)):
        issues.append(f"Install the FFmpeg features from dependencies/vcpkg.json for {triplet}.")
    return root, include, release, debug


def check(options):
    system, arch, triplet = platform_settings(options)
    env = os.environ.copy()
    issues = []
    if system == "Windows":
        from win.toolchain import prepare
        prepare(env, arch, issues)
    elif system == "Darwin":
        from mac.toolchain import prepare
        prepare(env, issues)
    else:
        from linux.toolchain import prepare
        prepare(env, options.flexaudio, issues)

    programs = {}

    def require(name, hint, minimum=None):
        path = shutil.which(name, path=env.get("PATH", ""))
        if not path:
            issues.append(hint)
            return
        programs[name] = path
        if minimum:
            result = subprocess.run([path, "--version"], capture_output=True, text=True, env=env)
            version = re.search(r"(\d+)\.(\d+)", result.stdout)
            if result.returncode or not version or tuple(map(int, version.groups())) < minimum:
                issues.append(hint)

    if system != "Windows":
        for variable, default, description in (("CC", "cc", "C"), ("CXX", "c++", "C++17")):
            compiler = env.get(variable) or default
            command = [compiler] if shutil.which(compiler, path=env.get("PATH")) else shlex.split(compiler)
            require(command[0] if command else default,
                    f"Install a {description} compiler or set {variable} to its executable.")
    if options.frontend == "cmake":
        require("cmake", "Install CMake 3.20 or newer and put it on PATH.", (3, 20))
        if options.tests:
            require("ctest", "Install CTest from the CMake package and put it on PATH.")
    elif options.frontend == "meson":
        require("meson", "Install Meson 1.3 or newer and put it on PATH.", (1, 3))
    else:
        require("scons", "Install SCons and put it on PATH.")
    if options.frontend in ("cmake", "meson"):
        require("ninja", "Install Ninja and put it on PATH.")
    if options.flexaudio:
        cargo_bin = Path(env.get("CARGO_HOME", Path.home() / ".cargo")) / "bin"
        if cargo_bin.is_dir() and not shutil.which("cargo", path=env.get("PATH", "")):
            env["PATH"] = os.pathsep.join((str(cargo_bin), env.get("PATH", "")))
        require("cargo", "Install Cargo and activate a Rust toolchain for FlexAudio.", (1, 85))
        require("rustc", "Install Rust 1.85 or newer for FlexAudio.", (1, 85))
        if system == "Windows":
            require("lib.exe", "Install the MSVC librarian used to build the FlexAudio bridge.")
    root, include, release, debug = ffmpeg_paths(options, system, triplet, issues)
    if issues:
        raise DependencyError("Missing or incompatible build dependencies:\n  - " + "\n  - ".join(issues))
    return BuildDependencies(env, programs, root, triplet, include, release, debug)


def describe(dependencies, options):
    directory = dependencies.debug_lib_dir if options.configuration == "Debug" else dependencies.release_lib_dir
    print(f"Build dependencies found for {platform.system()} ({options.frontend}, {options.configuration}).")
    print(f"FFmpeg headers: {dependencies.include_dir}")
    print(f"FFmpeg archives: {directory}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    add_options(parser)
    options = parser.parse_args()
    try:
        describe(check(options), options)
    except (DependencyError, OSError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
