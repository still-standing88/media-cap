from pathlib import Path
import shutil
import subprocess


def prepare(env, arch, issues):
    active_arch = env.get("VSCMD_ARG_TGT_ARCH", "").lower()
    if active_arch == "amd64":
        active_arch = "x64"
    if active_arch and active_arch != arch:
        issues.append(f"The Developer Command Prompt targets {active_arch}, but the build targets {arch}.")
        return
    if not env.get("VSCMD_VER"):
        vswhere = (Path(env.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
                   / "Microsoft Visual Studio" / "Installer" / "vswhere.exe")
        if not vswhere.is_file():
            issues.append("Install Visual Studio Build Tools with Desktop development with C++ and a Windows SDK.")
            return
        component = "ARM64" if arch == "arm64" else "x86.x64"
        installation = subprocess.run(
            [str(vswhere), "-latest", "-products", "*", "-requires",
             f"Microsoft.VisualStudio.Component.VC.Tools.{component}", "-property", "installationPath", "-utf8"],
            capture_output=True, text=True, encoding="utf-8", check=True, env=env,
        ).stdout.strip()
        if not installation:
            issues.append(f"Install the MSVC {arch} tools and Windows SDK in Visual Studio Installer.")
            return
        batch = Path(installation) / "Common7" / "Tools" / "VsDevCmd.bat"
        env["MEDIACAP_VSDEV_COMMAND"] = str(batch)
        target = "amd64" if arch == "x64" else arch
        result = subprocess.run(
            f'cmd.exe /d /u /c call "%MEDIACAP_VSDEV_COMMAND%" -no_logo -arch={target} >nul && set',
            capture_output=True, env=env,
        )
        del env["MEDIACAP_VSDEV_COMMAND"]
        if result.returncode:
            issues.append(f"Visual Studio could not activate the {arch} tools. Open its Developer Command Prompt.")
            return
        for line in result.stdout.decode("utf-16-le", errors="replace").splitlines():
            key, sep, value = line.partition("=")
            if sep and key:
                env[key.upper()] = value

    for name in ("cl.exe", "link.exe", "rc.exe"):
        if not shutil.which(name, path=env.get("PATH", "")):
            issues.append(f"{name} is missing. Install the MSVC toolset and Windows SDK.")

    cmake_tools = Path(env.get("VSINSTALLDIR", "")) / "Common7/IDE/CommonExtensions/Microsoft/CMake"
    extra = [cmake_tools / name for name in ("CMake/bin", "Ninja")]
    env["PATH"] = ";".join([env.get("PATH", "")] + [str(path) for path in extra if path.is_dir()])
