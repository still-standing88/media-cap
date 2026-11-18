"""Stage one installed CMake build for the GitHub release workflow."""

import argparse
from pathlib import Path
import shutil
import subprocess

from check_dependencies import FFMPEG_LIBRARIES


def require(path):
    if not path.is_file():
        raise SystemExit(f"Missing release input: {path}")
    return path


def add_linux_static_dependencies(package, triplet, vcpkg, destination):
    archive_dir = vcpkg / "installed" / triplet / "lib"
    dependencies = destination / "lib/mediacap-deps"
    dependencies.mkdir()
    names = []
    for name in FFMPEG_LIBRARIES:
        candidates = (archive_dir / f"lib{name}{suffix}.a" for suffix in ("", "-static"))
        archive = next((path for path in candidates if path.is_file()), None)
        if archive is None:
            raise SystemExit(f"Missing static FFmpeg dependency: {name} in {archive_dir}")
        shutil.copy2(archive, dependencies)
        names.append(archive.name)
    names.append(require(package / "lib/libmediacap_flexaudio.a").name)
    (destination / "lib/pkgconfig").mkdir()
    archives = " ".join("${libdir}/mediacap-deps/" + name for name in names[:-1])
    archives += " ${libdir}/" + names[-1]
    (destination / "lib/pkgconfig/MediaCap.pc").write_text(
        "prefix=${pcfiledir}/../..\nlibdir=${prefix}/lib\nincludedir=${prefix}/include\n\n"
        "Name: MediaCap\nDescription: Screen, window, camera and audio capture\nVersion: 1.0.0\n"
        "Libs: -L${libdir} -lMediaCap\nCflags: -I${includedir} -DMEDIACAP_STATIC\n"
        "Requires.private: x11 xext xtst xfixes xinerama alsa libpipewire-0.3\n"
        f"Libs.private: {archives} -latomic -ldl -lm -lrt -lutil -pthread\n",
        encoding="utf-8",
    )


def package_build(package, assets, triplet, configuration, linkage, vcpkg):
    windows = triplet.endswith("-windows-static")
    arch = triplet.split("-", 1)[0]
    platform = "windows" if windows else "linux"
    stem = f"MediaCap-library-{platform}-{arch}-{configuration.lower()}-{linkage}"
    staging = assets.parent / stem
    staging.mkdir(exist_ok=False)
    shutil.copytree(package / "include", staging / "include")
    shutil.copytree(package / "lib", staging / "lib")
    shutil.copytree(package / "share/licenses/MediaCap", staging / "share/licenses/MediaCap")

    if windows:
        require(staging / "lib/MediaCap.lib")
        require(staging / "lib/cmake/MediaCap/MediaCapConfig.cmake")
        if linkage == "shared":
            (staging / "bin").mkdir()
            shutil.copy2(require(package / "bin/MediaCap.dll"), staging / "bin")
            pdb = package / "bin/MediaCap.pdb"
            if configuration == "Debug" and pdb.is_file():
                shutil.copy2(pdb, staging / "bin")
        else:
            require(staging / "lib/mediacap-deps/mediacap_flexaudio.lib")
    elif linkage == "static":
        require(staging / "lib/libMediaCap.a")
        add_linux_static_dependencies(package, triplet, vcpkg, staging)
    else:
        require(staging / "lib/libMediaCap.so")

    if windows:
        shutil.make_archive(str(assets / stem), "zip", staging.parent, stem)
    else:
        subprocess.run(["tar", "-czf", str(assets / (stem + ".tar.gz")),
                        "-C", str(staging.parent), stem], check=True)

    if configuration == "Release" and linkage == "static":
        filename = "MediaCap.exe" if windows else "MediaCap"
        binary = require(package / "bin" / filename)
        cli_name = f"MediaCap-cli-{platform}-{arch}"
        shutil.copy2(binary, assets / (cli_name + (".exe" if windows else "")))
        notices = package / "share/licenses"
        with (assets / f"{cli_name}-notices.txt").open("wb") as output:
            require(notices / "MediaCapCLI/CLI11-LICENSE.txt")
            require(notices / "MediaCapCLI/nlohmann-json-LICENSE.txt")
            for source in sorted(notices.rglob("*")):
                if source.is_file():
                    output.write(f"{source.relative_to(notices).as_posix()}\n\n".encode())
                    output.write(source.read_bytes())
                    output.write(b"\n\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=(
        "x64-windows-static", "arm64-windows-static", "x64-linux", "arm64-linux"))
    parser.add_argument("--configuration", required=True, choices=("Release", "Debug"))
    parser.add_argument("--linkage", required=True, choices=("shared", "static"))
    parser.add_argument("--build-root", type=Path, default=Path("build-ci"))
    parser.add_argument("--vcpkg", type=Path, default=Path(".ci-vcpkg"))
    args = parser.parse_args()
    assets = args.build_root / "release-assets"
    assets.mkdir(exist_ok=False)
    package_build(args.build_root / "package", assets, args.platform, args.configuration, args.linkage, args.vcpkg)


if __name__ == "__main__":
    main()
