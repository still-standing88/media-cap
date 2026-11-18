import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VENV = ROOT / ".local/venv-docs"
DOXYGEN_DIR = ROOT / ".local/tools/doxygen"
DOXYGEN_VERSION = (1, 15, 0)


def environment_python(directory):
    return directory / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def find_doxygen(requested=None):
    selected = requested or os.environ.get("DOXYGEN")
    if selected:
        return shutil.which(selected)
    name = "doxygen.exe" if os.name == "nt" else "doxygen"
    local = DOXYGEN_DIR / name
    return str(local) if local.is_file() else shutil.which("doxygen")


def doxygen_version(executable):
    try:
        result = subprocess.run(
            [str(executable), "--version"], capture_output=True, text=True, check=True, timeout=15,
        )
    except (OSError, subprocess.SubprocessError):
        return ()
    match = re.match(r"(\d+)\.(\d+)\.(\d+)", result.stdout.strip())
    return tuple(map(int, match.groups())) if match else ()


def print_command(*parts):
    command = [str(part) for part in parts]
    print(subprocess.list2cmdline(command) if sys.platform == "win32" else shlex.join(command))
