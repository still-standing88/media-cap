"""Build MediaCap from the repository root using CMake, Meson, or SCons."""

import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

from check_dependencies import ROOT, DependencyError, add_options, check, describe
from build_dotnet import pack as pack_dotnet, stage_native, target_rid, verify_package


def enabled(value):
    return "true" if value else "false"


def flag(value):
    return "ON" if value else "OFF"


def build_cmake(options, deps, path, run):
    args = [
        "-S", str(ROOT), "-B", str(path), "-G", "Ninja",
        f"-DCMAKE_BUILD_TYPE={options.configuration}",
        f"-DBUILD_SHARED_LIBS={flag(options.shared)}",
        f"-DMEDIACAP_BUILD_CLI={flag(options.cli)}",
        f"-DMEDIACAP_BUILD_GUI={flag(options.gui)}",
        f"-DMEDIACAP_BUILD_PYTHON={flag(options.python)}",
        f"-DMEDIACAP_BUILD_TESTS={flag(options.tests)}",
        f"-DMEDIACAP_BUILD_EXAMPLES={flag(options.examples)}",
        f"-DMEDIACAP_FLEXAUDIO={flag(options.flexaudio)}",
        f"-DMEDIACAP_FLEXAUDIO_OFFLINE={flag(options.offline)}",
        f"-DMEDIACAP_USE_VCPKG={flag(deps.vcpkg_root is not None)}",
    ]
    if options.python:
        args.append(f"-DPython_EXECUTABLE={options.python_executable}")
    if options.gui and options.slint_root:
        args.append(f"-DSlint_ROOT={options.slint_root}")
    if deps.vcpkg_root:
        args.extend((f"-DMEDIACAP_VCPKG_ROOT={deps.vcpkg_root}", f"-DVCPKG_TARGET_TRIPLET={deps.triplet}"))
    else:
        args.extend((f"-DMEDIACAP_FFMPEG_INCLUDE_DIR={deps.include_dir}",
                     f"-DMEDIACAP_FFMPEG_LIBRARY_DIR={deps.release_lib_dir}",
                     f"-DMEDIACAP_FFMPEG_DEBUG_LIBRARY_DIR={deps.debug_lib_dir}"))
    run("cmake", *args)
    run("cmake", "--build", str(path), "--parallel", str(options.jobs))
    if options.tests:
        run("ctest", "--test-dir", str(path), "--output-on-failure", "--parallel", str(options.jobs))
    if options.install_prefix:
        run("cmake", "--install", str(path), "--prefix", str(options.install_prefix))


def build_meson(options, deps, path, run):
    args = ["setup"]
    if (path / "meson-private" / "coredata.dat").is_file():
        args.append("--reconfigure")
    args.extend((
        str(path), str(ROOT), f"--buildtype={options.configuration.lower()}",
        f"--default-library={'shared' if options.shared else 'static'}",
        f"-Dbuild_cli={enabled(options.cli)}", f"-Dbuild_tests={enabled(options.tests)}",
        f"-Dbuild_gui={enabled(options.gui)}",
        f"-Dbuild_python={enabled(options.python)}",
        f"-Dbuild_examples={enabled(options.examples)}",
        f"-Dflexaudio={'enabled' if options.flexaudio else 'disabled'}",
        f"-Dflexaudio_offline={enabled(options.offline)}",
        f"-Duse_vcpkg={enabled(deps.vcpkg_root is not None)}",
    ))
    if options.python:
        args.append(f"-Dpython_executable={options.python_executable}")
    if options.gui and options.slint_root:
        args.append(f"-Dslint_root={options.slint_root}")
    if deps.vcpkg_root:
        args.extend((f"-Dvcpkg_root={deps.vcpkg_root}", f"-Dvcpkg_triplet={deps.triplet}"))
    else:
        args.extend((f"-Dffmpeg_include_dir={deps.include_dir}", f"-Dffmpeg_lib_dir={deps.release_lib_dir}",
                     f"-Dffmpeg_debug_lib_dir={deps.debug_lib_dir}"))
    if options.install_prefix:
        args.append(f"--prefix={options.install_prefix}")
    run("meson", *args)
    run("meson", "compile", "-C", str(path), "-j", str(options.jobs))
    if options.tests:
        run("meson", "test", "-C", str(path), "--no-rebuild", "--print-errorlogs", "-j", str(options.jobs))
    if options.install_prefix:
        run("meson", "install", "-C", str(path), "--no-rebuild")


def build_scons(options, deps, path, run):
    args = [
        "-j", str(options.jobs), f"build_dir={path}",
        f"debug={int(options.configuration == 'Debug')}", f"shared={int(options.shared)}",
        f"cli={int(options.cli)}", f"tests={int(options.tests)}", f"examples={int(options.examples)}",
        f"gui={int(options.gui)}",
        f"python={int(options.python)}",
        f"flexaudio={int(options.flexaudio)}", f"flexaudio_offline={int(options.offline)}",
        f"use_vcpkg={int(deps.vcpkg_root is not None)}",
    ]
    if options.python:
        args.append(f"python_executable={options.python_executable}")
    if options.gui and options.slint_root:
        args.append(f"slint_root={options.slint_root}")
    if deps.vcpkg_root:
        args.extend((f"vcpkg_root={deps.vcpkg_root}", f"vcpkg_triplet={deps.triplet}"))
    else:
        args.extend((f"ffmpeg_include_dir={deps.include_dir}", f"ffmpeg_lib_dir={deps.release_lib_dir}",
                     f"ffmpeg_debug_lib_dir={deps.debug_lib_dir}"))
    run("scons", *args)
    if options.install_prefix:
        install_args = [argument for argument in args if not argument.startswith("tests=")]
        run("scons", *install_args, "tests=0", f"prefix={options.install_prefix}", "install")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    add_options(parser)
    parser.add_argument("--jobs", type=int, default=max(1, min(os.cpu_count() or 1, 4)))
    parser.add_argument("--build-dir", type=Path, help="Use this build directory")
    parser.add_argument("--install-prefix", type=Path, help="Install into this directory after building")
    parser.add_argument("--check-only", action="store_true", help="Validate dependencies without building")
    parser.add_argument("--gui", action="store_true", help="Build the AudioPix Slint application")
    parser.add_argument("--slint-root", type=Path, default=os.environ.get("SLINT_ROOT"),
                        help="External Slint C++ SDK prefix for AudioPix")
    parser.add_argument("--python", action="store_true", help="Build the MediaCap CPython extension")
    parser.add_argument("--python-executable", default=sys.executable, help="Interpreter for the CPython extension")
    parser.add_argument("--dotnet", action="store_true", help="Pack .NET bindings with the shared MediaCap library")
    options = parser.parse_args()
    if options.slint_root:
        options.slint_root = options.slint_root.resolve()
    if options.jobs < 1:
        parser.error("--jobs must be at least 1")
    if options.dotnet and not options.shared:
        parser.error("--dotnet requires a shared native library")
    variant = f"{options.configuration.lower()}-{'shared' if options.shared else 'static'}"
    default = ROOT / ("build-scons" if options.frontend == "scons" else "build")
    if options.frontend != "scons":
        variant = f"{options.frontend}-{variant}"
    path = (options.build_dir or default / variant).resolve()
    if options.install_prefix:
        options.install_prefix = options.install_prefix.resolve()
    elif options.dotnet:
        options.install_prefix = path / "dotnet" / "install"
    try:
        dependencies = check(options)
        describe(dependencies, options)
        if options.dotnet:
            dotnet = shutil.which("dotnet", path=dependencies.environment.get("PATH"))
            if not dotnet:
                raise DependencyError("The .NET 10 SDK is required for --dotnet.")
            dependencies.programs["dotnet"] = dotnet
            rid = target_rid(dependencies.triplet)
        if options.check_only:
            return 0
        dependencies.environment.setdefault("CARGO_BUILD_JOBS", str(options.jobs))

        def run(program, *args):
            command = [dependencies.programs[program], *args]
            display = subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)
            print(f"Running {display}", flush=True)
            subprocess.run(command, cwd=ROOT, env=dependencies.environment, check=True)

        builders = {"cmake": build_cmake, "meson": build_meson, "scons": build_scons}
        builders[options.frontend](options, dependencies, path, run)
        if options.dotnet:
            assets = path / "dotnet" / "runtimes"
            native = stage_native(options.install_prefix, assets, rid)
            print(f"Staged {native}")
            packages = path / "dotnet" / "packages"
            pack_dotnet(run, assets, packages, options.configuration)
            if options.tests:
                verify_package(run, packages, path / "dotnet" / "tests", options.configuration,
                               dependencies.environment)
        print(f"MediaCap build complete: {path}")
    except subprocess.CalledProcessError as error:
        print(f"Build command exited with status {error.returncode}.", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except (DependencyError, OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
