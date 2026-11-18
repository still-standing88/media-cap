#!/usr/bin/env python3
"""Install the Python documentation tools and an available Doxygen binary."""
import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.request import urlopen
import venv
import zipfile

from docs_tools import (
    DEFAULT_VENV, DOXYGEN_DIR, DOXYGEN_VERSION, ROOT, doxygen_version,
    environment_python, find_doxygen, print_command,
)

RELEASE = "https://github.com/doxygen/doxygen/releases/download/Release_1_18_0/"
ASSETS = {
    ("win32", "x86_64"): (
        "doxygen-1.18.0.windows.x64.bin.zip",
        "e84f54cfd49ef06b0b16536056dbec0c496323de28abcce53a4269463de35eaf",
        ("libclang.dll", "doxygen.exe"),
    ),
    ("linux", "x86_64"): (
        "doxygen-1.18.0.linux.bin.tar.gz",
        "14fa81bdc34171edb5f1f02b1d60e74802f0439b77fa44e592565d517d72df90",
        ("doxygen",),
    ),
    ("darwin", "arm64"): (
        "doxygen-1.18.0-mac-arm.zip",
        "31d1c74467a9f6f456b4e6a4eff0c03e93d1ffa92da5fdf2d53a2f14ebaabbd7",
        ("doxygen",),
    ),
    ("darwin", "x86_64"): (
        "doxygen-1.18.0-mac-intel.zip",
        "8045d72f6f900fd430042339c1493d6eefcc85a7f990361fb57685682f16a2f9",
        ("doxygen",),
    ),
}


def install_doxygen():
    machine = platform.machine().lower()
    machine = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
    asset = ASSETS.get((sys.platform, machine))
    if asset is None:
        raise SystemExit(
            f"No portable Doxygen download for {sys.platform}/{machine}. "
            "Install Doxygen 1.15 or newer with your package manager or build it from "
            "https://www.doxygen.nl/download.html, then pass --doxygen."
        )
    name, expected, members = asset
    print(f"Downloading {name}", flush=True)
    with tempfile.TemporaryDirectory(prefix="mediacap-doxygen-") as temporary:
        download = Path(temporary) / name
        with urlopen(RELEASE + name, timeout=60) as response, download.open("wb") as output:
            shutil.copyfileobj(response, output)
        with download.open("rb") as stream:
            if hashlib.file_digest(stream, "sha256").hexdigest() != expected:
                raise SystemExit("Doxygen archive checksum mismatch")
        is_zip = name.endswith(".zip")
        archive = zipfile.ZipFile(download) if is_zip else tarfile.open(download, "r:gz")
        with archive:
            names = (
                [entry.filename for entry in archive.infolist() if not entry.is_dir()]
                if is_zip else [entry.name for entry in archive.getmembers() if entry.isfile()]
            )
            DOXYGEN_DIR.mkdir(parents=True, exist_ok=True)
            for member in members:
                matches = [entry for entry in names if PurePosixPath(entry).name == member]
                if len(matches) != 1:
                    raise SystemExit(f"Expected one {member} in {name}, found {len(matches)}")
                path = DOXYGEN_DIR / member
                staged = path.with_suffix(path.suffix + ".download")
                source = archive.open(matches[0]) if is_zip else archive.extractfile(matches[0])
                with source, staged.open("wb") as output:
                    shutil.copyfileobj(source, output)
                if sys.platform != "win32":
                    staged.chmod(0o755)
                staged.replace(path)
    return DOXYGEN_DIR / members[-1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--venv", type=Path, default=DEFAULT_VENV, help="Documentation virtual environment")
    parser.add_argument("--doxygen", help="Use this Doxygen executable instead of downloading one")
    args = parser.parse_args()
    if sys.version_info < (3, 11):
        parser.error("Python 3.11 or newer is required")

    executable = find_doxygen(args.doxygen)
    if not executable or doxygen_version(executable) < DOXYGEN_VERSION:
        if args.doxygen or os.environ.get("DOXYGEN"):
            parser.error("The selected Doxygen must be version 1.15 or newer")
        executable = install_doxygen()
        if doxygen_version(executable) < DOXYGEN_VERSION:
            parser.error(f"{executable} could not run. Install Doxygen for this system and pass --doxygen.")

    directory = args.venv.resolve()
    python = environment_python(directory)
    if not python.is_file():
        venv.EnvBuilder(with_pip=True).create(directory)
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(ROOT / "docs/requirements.txt")], check=True)
    print(f"Doxygen: {executable}")
    print(f"Documentation Python: {python}")
    print("Build the documentation with")
    print_command(sys.executable, ROOT / "scripts/build_api_docs.py", "--venv", directory,
                  "--doxygen", executable)


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
