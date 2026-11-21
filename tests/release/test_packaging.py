"""Exercise the release matrix and downloadable packages without compiling code."""

import os
from pathlib import Path
import re
import sys
import tarfile
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from check_dependencies import FFMPEG_LIBRARIES
from package_release import package_build, package_gui


def write(root, name, content=b"fixture", executable=False):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    if executable:
        path.chmod(0o755)
    return path


class ReleasePackagingTests(unittest.TestCase):
    def test_matrix_matches_readme_downloads_and_preserves_payloads(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            assets = root / "assets"
            assets.mkdir()
            slint = root / "slint"
            write(slint, "licenses/LICENSE.md", b"Slint license")
            write(slint, "licenses/THIRDPARTY.md", b"Slint dependency notices")
            for platform in ("windows", "linux"):
                for arch in ("x64", "arm64"):
                    triplet = f"{arch}-{'windows-static' if platform == 'windows' else 'linux'}"
                    for configuration, linkage in (("Release", "shared"), ("Release", "static"), ("Debug", "shared")):
                        with self.subTest(platform=platform, arch=arch, configuration=configuration, linkage=linkage):
                            build = root / f"{triplet}-{configuration}-{linkage}"
                            package = build / "package"
                            output = build / "release-assets"
                            output.mkdir(parents=True)
                            write(package, "include/MediaCap.h")
                            write(package, "share/licenses/MediaCap/LICENSE", b"MediaCap license")
                            if platform == "windows":
                                write(package, "lib/MediaCap.lib")
                                write(package, "lib/cmake/MediaCap/MediaCapConfig.cmake")
                                if linkage == "shared":
                                    write(package, "bin/MediaCap.dll")
                                else:
                                    write(package, "lib/mediacap-deps/mediacap_flexaudio.lib")
                            elif linkage == "shared":
                                write(package, "lib/libMediaCap.so")
                            else:
                                write(package, "lib/libMediaCap.a")
                                write(package, "lib/libmediacap_flexaudio.a")
                                for dependency in FFMPEG_LIBRARIES:
                                    write(build / "vcpkg", f"installed/{triplet}/lib/lib{dependency}.a")
                            gui = configuration == "Release" and linkage == "static"
                            if gui:
                                suffix = ".exe" if platform == "windows" else ""
                                write(package, f"bin/MediaCap{suffix}", b"static CLI payload", executable=True)
                                write(package, f"bin/AudioPix{suffix}", b"GUI payload", executable=True)
                                runtime = "slint_cpp.dll" if platform == "windows" else "libslint_cpp.so"
                                write(package, f"bin/{runtime}", b"Slint runtime")
                                if platform == "windows":
                                    write(package, "bin/vcruntime140.dll", b"MSVC runtime")
                                    write(package, "bin/msvcp140.dll", b"MSVC C++ runtime")
                                write(package, "share/licenses/MediaCapCLI/CLI11-LICENSE.txt", b"CLI11 license")
                                write(package, "share/licenses/MediaCapCLI/nlohmann-json-LICENSE.txt", b"JSON license")
                            package_build(package, output, triplet, configuration, linkage,
                                          build / "vcpkg", slint if gui else None)
                            for asset in output.iterdir():
                                self.assertFalse((assets / asset.name).exists(), f"Duplicate release asset: {asset.name}")
                                asset.rename(assets / asset.name)

            names = {path.name for path in assets.iterdir()}
            self.assertEqual(len(names), 24)
            readme = (ROOT / "README.md").read_text(encoding="utf-8")
            linked = set(re.findall(r"releases/latest/download/(\S+)", readme))
            self.assertEqual(linked, {name for name in names if not name.endswith("-notices.txt")})
            for platform in ("windows", "linux"):
                for arch in ("x64", "arm64"):
                    stem = f"MediaCap-cli-{platform}-{arch}"
                    suffix = ".exe" if platform == "windows" else ""
                    self.assertEqual((assets / (stem + suffix)).read_bytes(), b"static CLI payload")
                    notices = (assets / f"{stem}-notices.txt").read_text()
                    self.assertIn("CLI11 license", notices)
                    self.assertIn("JSON license", notices)
                    self.assertNotIn("Slint", notices)
                    gui_stem = f"AudioPix-{platform}-{arch}"
                    if platform == "windows":
                        with zipfile.ZipFile(assets / f"{gui_stem}.zip") as archive:
                            payload = {name: archive.read(name) for name in archive.namelist() if not name.endswith("/")}
                        self.assertEqual(payload[f"{gui_stem}/bin/slint_cpp.dll"], b"Slint runtime")
                        self.assertIn(f"{gui_stem}/bin/vcruntime140.dll", payload)
                        self.assertIn(f"{gui_stem}/bin/msvcp140.dll", payload)
                    else:
                        with tarfile.open(assets / f"{gui_stem}.tar.gz") as archive:
                            payload = {member.name: archive.extractfile(member).read()
                                       for member in archive.getmembers() if member.isfile()}
                            if os.name != "nt":
                                self.assertTrue(archive.getmember(f"{gui_stem}/bin/AudioPix").mode & 0o111)
                        self.assertEqual(payload[f"{gui_stem}/bin/libslint_cpp.so"], b"Slint runtime")
                    self.assertEqual(payload[f"{gui_stem}/bin/AudioPix{suffix}"], b"GUI payload")
                    self.assertEqual(payload[f"{gui_stem}/share/licenses/Slint/THIRDPARTY.md"], b"Slint dependency notices")
                    self.assertFalse(any(Path(name).name in ("MediaCap", "MediaCap.exe", "MediaCap.dll") for name in payload))

    def test_gui_cannot_be_published_from_shared_or_debug_builds(self):
        for configuration, linkage in (("Release", "shared"), ("Debug", "static")):
            with self.subTest(configuration=configuration, linkage=linkage):
                with self.assertRaisesRegex(SystemExit, "Release static"):
                    package_build(Path("unused"), Path("unused"), "x64-linux", configuration,
                                  linkage, Path("unused"), Path("slint"))

    def test_gui_requires_bundled_runtime(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write(root / "package", "bin/AudioPix.exe")
            with self.assertRaisesRegex(SystemExit, "slint_cpp.dll"):
                package_gui(root / "package", root / "assets", "windows", "x64", root / "slint")

    def test_windows_gui_requires_bundled_msvc_runtime(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write(root / "package", "bin/AudioPix.exe")
            write(root / "package", "bin/slint_cpp.dll")
            with self.assertRaisesRegex(SystemExit, "msvcp140.dll"):
                package_gui(root / "package", root / "assets", "windows", "x64", root / "slint")


if __name__ == "__main__":
    unittest.main()
