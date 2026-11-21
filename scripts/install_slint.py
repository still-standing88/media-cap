"""Download and verify the pinned Slint C++ SDK used by release builds."""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request


VERSION = "1.17.0"
SDK_ASSETS = {
    "x64-windows-static": ("win64-MSVC-AMD64.exe", "d9a2d6fb98bff1f6ac1c2b95673f06cbe7c5d1db30e1c7d345c248cdd1640e08"),
    "arm64-windows-static": ("win64-MSVC-ARM64.exe", "aacf180ddbf3acdcfdad54867906b5f072f3581b457ee5e752cb6f1d511e3847"),
    "x64-linux": ("Linux-x86_64.tar.gz", "4de40322dee9c425d95f30f76219522a181001477d0e9c6dd2c7d72fc7894224"),
    "arm64-linux": ("Linux-arm64.tar.gz", "2dc3f1477667cfade375dd033d31a8f101db0799d77470344f0a3cb417a1bc69"),
}
COMPILER_ASSETS = {
    # The Linux ARM64 SDK ships its runtime only; use the native compiler asset.
    "arm64-linux": ("slint-compiler-Linux-aarch64.tar.gz", "9108b27b50f266ddac9773bf67b45233a27216345e9e2b010c381d79909f0549"),
}


def download(directory, filename, checksum):
    archive = directory / filename
    url = f"https://github.com/slint-ui/slint/releases/download/v{VERSION}/{filename}"
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(url) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
        raise SystemExit(f"Slint checksum mismatch: {filename}")
    return archive


def install(platform, destination):
    suffix, checksum = SDK_ASSETS[platform]
    filename = f"Slint-cpp-{VERSION}-{suffix}"
    destination = destination.resolve()
    if destination.exists():
        raise SystemExit(f"SDK destination already exists: {destination}")
    with tempfile.TemporaryDirectory() as temporary:
        archive = download(Path(temporary), filename, checksum)
        unpacked = Path(temporary) / "sdk"
        unpacked.mkdir()
        if filename.endswith(".exe"):
            # Unpack the NSIS installer without running it or changing the host.
            seven_zip = shutil.which("7z") or shutil.which("7z.exe")
            if not seven_zip:
                raise SystemExit("7-Zip is required to extract the Windows Slint SDK")
            subprocess.run([seven_zip, "x", str(archive), f"-o{unpacked}", "-y"], check=True)
            prefix = unpacked
        else:
            with tarfile.open(archive) as sdk:
                sdk.extractall(unpacked, filter="data")
            prefix = unpacked / filename.removesuffix(".tar.gz")
        if platform in COMPILER_ASSETS:
            compiler_archive = download(Path(temporary), *COMPILER_ASSETS[platform])
            with tarfile.open(compiler_archive) as compiler:
                compiler.extractall(prefix / "bin", filter="data")
        for directory in ("bin", "include", "lib", "licenses"):
            if not (prefix / directory).is_dir():
                raise SystemExit(f"Missing Slint SDK directory: {directory}")
        if not (prefix / "lib/cmake/Slint/SlintConfig.cmake").is_file():
            raise SystemExit("Missing Slint CMake package")
        compiler_name = "slint-compiler.exe" if platform.endswith("windows-static") else "slint-compiler"
        if not (prefix / "bin" / compiler_name).is_file():
            raise SystemExit("Missing native Slint compiler")
        destination.mkdir(parents=True)
        for directory in ("bin", "include", "lib", "licenses"):
            shutil.copytree(prefix / directory, destination / directory)
    print(f"Installed Slint {VERSION} in {destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=SDK_ASSETS)
    parser.add_argument("--destination", type=Path, default=Path("build-ci/slint"))
    args = parser.parse_args()
    install(args.platform, args.destination)


if __name__ == "__main__":
    main()
