"""Locate the shared-library layout used by the Slint C++ desktop SDK."""

import os
from pathlib import Path
import re
import subprocess
import sys


def locate(prefix, system, environment=None):
    environment = os.environ if environment is None else environment
    prefix = prefix or environment.get("SLINT_ROOT")
    if not prefix:
        raise ValueError("AudioPix requires a Slint C++ 1.17+ SDK. Set slint_root or SLINT_ROOT.")
    root = Path(prefix).expanduser().resolve()
    include = root / "include" / "slint"
    bin_dir = root / "bin"
    lib_dirs = (root / "lib", root / "lib64", bin_dir)
    compiler = bin_dir / ("slint-compiler.exe" if os.name == "nt" else "slint-compiler")
    if system == "windows":
        runtime_name, link_name = "slint_cpp.dll", "slint_cpp.dll.lib"
    else:
        runtime_name = "libslint_cpp.dylib" if system == "darwin" else "libslint_cpp.so"
        link_name = runtime_name

    def lookup(name):
        for directory in lib_dirs:
            path = directory / name
            if path.is_file():
                return path
        raise ValueError(f"Could not find {name} in the Slint SDK at {root}.")

    if not (include / "slint.h").is_file() or not compiler.is_file():
        raise ValueError(f"Expected the Slint C++ headers and compiler under {root}.")
    version = subprocess.check_output([compiler, "--version"], text=True, env=environment)
    match = re.search(r"slint-compiler (\d+)\.(\d+)", version)
    if not match or tuple(map(int, match.groups())) < (1, 17):
        raise ValueError(f"AudioPix requires Slint 1.17 or newer. Found {version.strip()}.")
    return compiler, include, lookup(link_name), lookup(runtime_name)


if __name__ == "__main__":
    try:
        for item in locate(sys.argv[1], sys.argv[2]):
            sys.stdout.buffer.write(os.fsencode(item) + b"\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        sys.exit(str(error))
