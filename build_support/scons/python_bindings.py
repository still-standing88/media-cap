"""Build and stage the CPython extension using the normal library consumer environment."""
import json
import os
from pathlib import Path
import subprocess

from SCons.Errors import UserError
from SCons.Script import Action

from targets import objects, read_list


def build_python(env, consumer, output, library, is_windows, is_macos):
    command = [env["python_executable"], "bindings/python/python_config.py"]
    try:
        metadata = json.loads(subprocess.check_output(command, env=env["ENV"], text=True))
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        raise UserError(f"Cannot inspect binding interpreter {command[0]} (install pybind11 there): {error}") from error
    if not metadata["suffix"]:
        raise UserError("The binding interpreter must report an extension module suffix")

    extension = consumer.Clone()
    extension.Append(CPPPATH=metadata["include"])
    extension.Replace(LDMODULEPREFIX="", LDMODULESUFFIX=metadata["suffix"])
    package_dir = Path(metadata["platlib"]) / "mediacap"
    package = output + "python/mediacap/"
    if is_windows:
        extension.Append(CXXFLAGS=["/bigobj"])
        expected_platforms = {"x64": "win-amd64", "arm64": "win-arm64", "x86": "win32"}
        arch = env["TARGET_ARCH"]
        platform = expected_platforms.get(arch)
        if platform and platform != metadata["platform"]:
            raise UserError(f"MSVC target {arch} does not match the Python platform {metadata['platform']}")
        library_path = Path(metadata["libdir"]) / metadata["library"]
        if not library_path.is_file():
            raise UserError(f"Python import library not found: {library_path}")
        extension.Append(LIBS=[extension.File(str(library_path))])
    else:
        extension.Append(CXXFLAGS=["-fvisibility=hidden"])
        if is_macos:
            extension.Append(LDMODULEFLAGS=["-Wl,-undefined,dynamic_lookup"])
        if env["shared"]:
            root_lib = os.path.relpath("lib", package_dir).replace(os.sep, "/")
            loader = "@loader_path" if is_macos else r"\$$ORIGIN"
            extension.Replace(RPATH=[])
            extension.Append(LDMODULEFLAGS=[f"-Wl,-rpath,{loader}/{root_lib}"])
            # The corresponding relative route in the build tree.
            extension.Append(LDMODULEFLAGS=[f"-Wl,-rpath,{loader}/../.."])
        elif not is_macos:
            extension.Append(LDMODULEFLAGS=["-Wl,--exclude-libs,ALL"])

    sources = ["bindings/python/" + name for name in read_list("bindings/python/sources.txt")]
    module = extension.LoadableModule(package + "_native", objects(extension, sources, output, shared=True))
    package_sources = ["bindings/python/mediacap/" + name for name in read_list("bindings/python/package_files.txt")]
    build_files = [module[0], *extension.Install(package, package_sources)]
    if env["shared"] and is_windows:
        dll = next(node for node in library if str(node).lower().endswith(".dll"))
        build_files.extend(extension.Install(package, dll))
    return build_files, package_dir


def add_python_test(env, output, python_files):
    def run_test(target, source, env):
        environment = dict(env["ENV"], PYTHONPATH=env.Dir(output + "python").get_abspath())
        return subprocess.call([env["python_executable"], "tests/integration/python_bindings.py"], env=environment)

    target = env.Alias("python-bindings-test", python_files + ["tests/integration/python_bindings.py"], Action(run_test))
    env.AlwaysBuild(target)
    return target
