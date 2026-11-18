from pathlib import Path
import shutil
import subprocess


def prepare(env, issues):
    xcrun = shutil.which("xcrun", path=env.get("PATH", ""))
    if not xcrun:
        issues.append("Install Xcode Command Line Tools with xcode-select --install.")
        return
    result = subprocess.run([xcrun, "--sdk", "macosx", "--show-sdk-path"],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        issues.append("Select an installed macOS SDK with xcode-select.")
        return
    frameworks = Path(result.stdout.strip()) / "System" / "Library" / "Frameworks"
    for framework in ("AVFoundation", "CoreAudio", "CoreGraphics", "ImageIO", "VideoToolbox"):
        if not (frameworks / f"{framework}.framework").is_dir():
            issues.append(f"The selected macOS SDK is missing {framework}.framework.")
