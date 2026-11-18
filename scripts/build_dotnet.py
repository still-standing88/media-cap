"""Stage an installed MediaCap shared library and pack the .NET bindings."""
import argparse
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "bindings/dotnet/MediaCap/MediaCap.csproj"
TESTS = ROOT / "tests/integration/dotnet"
RUNTIME_NAMES = {
    "win": ("MediaCap.dll", "MediaCap.Native.dll"),
    "linux": ("libMediaCap.so", "libMediaCap.Native.so"),
    "osx": ("libMediaCap.dylib", "libMediaCap.Native.dylib"),
}
RIDS = tuple(f"{os}-{arch}" for os in RUNTIME_NAMES for arch in ("x64", "arm64"))


def target_rid(triplet, system=None):
    os = {"Windows": "win", "Linux": "linux", "Darwin": "osx"}.get(system or platform.system())
    rid = f"{os}-{triplet.split('-', 1)[0]}"
    if rid not in RIDS:
        raise ValueError(f"MediaCap .NET does not have a runtime asset layout for {triplet}")
    return rid


def stage_native(prefix, assets, rid):
    os = rid.split("-", 1)[0]
    native_name, packed_name = RUNTIME_NAMES[os]
    if os == "win":
        native = prefix / "bin" / native_name
    else:
        # Meson can use a multiarch library directory under lib.
        candidates = [path for pattern in (f"lib*/{native_name}", f"lib*/*/{native_name}")
                      for path in sorted(prefix.glob(pattern)) if path.is_file()]
        native = next(iter(candidates), prefix / "lib" / native_name)
    if not native.is_file():
        raise ValueError(f"Can't find the installed MediaCap shared library: {native}")
    destination = assets / rid / "native" / packed_name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(native, destination)
    return destination


def pack(run, assets, output, configuration):
    run("dotnet", "pack", str(PROJECT), "--configuration", configuration, "--output", str(output),
        "--artifacts-path", str(output.parent / "managed"), f"-p:MediaCapNativeAssets={assets}")


def verify_package(run, packages, work, configuration, environment):
    version = ET.parse(PROJECT).findtext("./PropertyGroup/Version")
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=work) as temporary:
        root = Path(temporary)
        source = TESTS / "abi_probe.c"
        headers = ROOT / "include"
        if platform.system() == "Windows":
            probe = root / "abi_probe.exe"
            compiler = shutil.which("cl", path=environment.get("PATH"))
            if not compiler:
                raise ValueError("The MSVC compiler is required to run the .NET ABI test.")
            command = [compiler, "/nologo", f"/I{headers}", str(source),
                       f"/Fo{root / 'abi_probe.obj'}", f"/Fe{probe}"]
        else:
            probe = root / "abi_probe"
            compiler = environment.get("CC") or "cc"
            command = [compiler] if shutil.which(compiler, path=environment.get("PATH")) else shlex.split(compiler)
            command += ["-std=c11", "-I", str(headers), str(source), "-o", str(probe)]
        subprocess.run(command, cwd=root, env=environment, check=True)
        run("dotnet", "run", "--project", str(TESTS / "MediaCap.Tests.csproj"),
            "--configuration", configuration, "--artifacts-path", str(root / "artifacts"),
            f"-p:MediaCapPackageVersion={version}", f"-p:RestoreSources={packages}",
            f"-p:RestorePackagesPath={root / 'cache'}", "--", "--abi", str(probe),
            *(["--screen"] if environment.get("MEDIACAP_TEST_SCREEN") == "1" else []))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True, help="Root of the staged RID directories")
    parser.add_argument("--prefix", type=Path, help="Staged install prefix produced by a native build")
    parser.add_argument("--rid", choices=RIDS, help="Runtime identifier for the staged native build")
    parser.add_argument("--output", type=Path, help="Directory in which to write the NuGet package")
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Release")
    options = parser.parse_args()
    if bool(options.prefix) != bool(options.rid):
        parser.error("--prefix and --rid must be used together")
    if not options.prefix and not options.output:
        parser.error("request staging with --prefix or packing with --output")

    try:
        assets = options.assets.resolve()
        if options.prefix:
            print(f"Staged {stage_native(options.prefix.resolve(), assets, options.rid)}")
        if options.output:
            if not any((assets / rid / "native" / RUNTIME_NAMES[rid.split("-", 1)[0]][1]).is_file()
                       for rid in RIDS):
                raise ValueError(f"No staged MediaCap native assets in {assets}")
            pack(lambda *args: subprocess.run(args, cwd=ROOT, check=True),
                 assets, options.output.resolve(), options.configuration)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
