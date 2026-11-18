"""Report build metadata for the interpreter that will import the extension."""
import json
import os
from pathlib import Path
import sys
import sysconfig


def platlib():
    prefix = os.path.abspath("mediacap-python-prefix")
    path = sysconfig.get_path("platlib", vars={"base": prefix, "platbase": prefix})
    return Path(os.path.relpath(path, prefix)).as_posix()


def build_config():
    import pybind11

    paths = sysconfig.get_paths()
    return {
        "include": list(dict.fromkeys([paths["include"], paths["platinclude"], pybind11.get_include()])),
        "suffix": sysconfig.get_config_var("EXT_SUFFIX"),
        "platform": sysconfig.get_platform(),
        "libdir": str(Path(sys.base_exec_prefix) / "libs"),
        "library": sysconfig.get_config_var("LIBRARY") or f"python{sys.version_info.major}{sys.version_info.minor}.lib",
        "platlib": platlib(),
    }


if __name__ == "__main__":
    print(platlib() if sys.argv[1:] == ["platlib"] else json.dumps(build_config()))
