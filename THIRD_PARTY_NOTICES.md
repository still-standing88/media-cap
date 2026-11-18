# MediaCap licensing and third-party notices

MediaCap original code is Copyright (c) 2026 MediaCap contributors and licensed
under **GNU LGPL version 3 or, at your option, any later version**. See LICENSE.
COPYING supplies the GPL version 3 terms incorporated by LGPL version 3; it does
not indicate that GPL-only codecs are included. There is no warranty.

The LGPLv3 choice accommodates Apache-2.0 dependencies in flexaudio's CPAL
backend. Vendored and linked third-party code keeps its own license and copyright.
The following are included in the Windows capture build:

| Component | License | Source / notices |
|---|---|---|
| CameraCapture | MIT | https://github.com/wysaid/CameraCapture ; `licenses/CameraCapture-MIT.txt` (source attribution in vendored headers) |
| screen_capture_lite | MIT | https://github.com/smasherprog/screen_capture_lite ; `licenses/screen_capture_lite-MIT.txt` |
| miniaudio | Public domain or MIT-0 | https://github.com/mackron/miniaudio ; `licenses/miniaudio.txt`; local changes in `src/third_party/miniaudio/MEDIACAP_PATCHES.md` |
| flexaudio 0.8.1 | MIT | https://github.com/Studio-Sadola/flexaudio ; commit `65e07f0dbf06b987d7af27064da6aca342e62e91`; `licenses/flexaudio-MIT.txt` |
| FFmpeg 7.1.1 | LGPL-2.1-or-later, built without GPL/nonfree | https://ffmpeg.org ; `licenses/vcpkg/ffmpeg/` |
| LAME 3.100 | LGPL (source files permit later versions) | https://lame.sourceforge.io ; `licenses/vcpkg/mp3lame/` |
| libaom, dav1d, libvpx, OpenH264, Opus, Ogg, Vorbis | BSD-style licenses; see full terms and patent notices | `licenses/vcpkg/` |
| CPAL | Apache-2.0 | `licenses/rust/cpal-*/` |
| nnnoiseless | BSD-3-Clause | `licenses/rust/nnnoiseless-*/` |
| Other Rust dependencies | MIT, Apache-2.0, BSD or Unicode terms | Versioned full notices in `licenses/rust/`; inventory in `licenses/manifest.json` |

The upstream flexaudio notice file describes its entire project, including
optional software not linked into MediaCap. The actual bridge is limited to
capture and its dependencies recorded in `src/integrations/flexaudio_bridge/Cargo.lock`.
CLI, full upstream FFI, VAD/ONNX and upstream encoding features are not built.

The statically linked MediaCap CLI uses CLI11 2.7.2 under BSD-3-Clause, from
https://github.com/CLIUtils/CLI11. Its full license is in
`cli/third_party/CLI11/LICENSE` and is installed with the CLI. CLI11 is not
linked into the capture library. The CLI also uses JSON for Modern C++ 3.12.0
under the MIT license in `cli/third_party/nlohmann/LICENSE`.
Documentation generators are development tools,
not library or CLI runtime dependencies.

Windows SDK import libraries refer to operating-system components and are build
inputs. End users do not need the SDK or Rust. Microsoft runtime redistribution
terms apply to the statically linked Visual C++ runtime. Only Release builds are
intended for distribution; Debug builds are developer artifacts.

See the LGPL in `LICENSE` for source availability, modification, replacement
and relinking requirements.
