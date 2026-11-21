# MediaCap

You shouldn't have to download half a Gigabyte of an application just to record the audio of a process or a window's application, nor should you have to install a virtual device just for that single purpose either. Existing tools don't give that much functionality. Some of this exists as a native functionality of certain platforms and just needs to be exposed in an elegant interface for both developers and users.

I started this project back in the summer of 2024, but I wasn't able to finish it due to the tremendous amount of workload I had from university and being involved in a startup.

Half of the credit goes to Claude for helping me finally put the final caps on this thing so I can actually get out there to the world. Testing is still needed across platforms; the release workflow builds ready-to-use Windows and Linux downloads.

This library doesn't just capture processes for audio but virtually anything, anything you could think of that a computer inputs or outputs.

Cameras, monitors, windows, audio form input and output devices, audio from any process that outputs it, and screenshots. The library allows you to output audio and video with the sample rate and frame rate of your choosing, in MKV/WebM/MP4 and MP3/FLAC/M4A. All that and more. The library has bindings for .NET and Python and has a C and a C++ API, which you can use the port it to other languages.

This library is more or less a ducktape and stitching that brings together several libraries, which makes all of this functionality possible. This lib wouldn't exist without their effort. Any modifications made to those libraries are merely small modifications to adapt them to the code base.

This library doesn't provide any editing or additional capabilities beyond capturing and saving into files; you've got OBS Studio for that. This is meant to be a quick, convenient, and straightforward solution when you just want to capture something and forward it to someone on the fly, as an example. In addition, this library uses Cisco's more license permissive H.264 implementation, which might not be the best. If you're looking for the high-quality version for that codec, you'll not find it here. This library is LGPL only and will remain so for the foreseeable future. No other components with incompatible licenses will be considered.

## Features

- Capture monitors, cameras, microphones, and speakers.
- Capture application windows or audio from individual processes.
- Capture part of the screen or take a screenshot.
- Save audio as MP3, FLAC, or AAC in a supported container.
- Save video as MKV, MP4, or WebM.
- Adjust frame rates, sample rates, and other recording settings.
- Works across Linux, macOS, and Windows, with some features dependent on the platform.

## What’s available?

MediaCap is available as a library with its own API, as a CLI tool, and as the AudioPix desktop application. The library and tools are still in beta while testing continues to uncover remaining issues.

## Downloads

Downloads will be available from the [latest release](https://github.com/still-standing88/media-cap/releases/latest) when the public release is ready. Choose the asset for your CPU architecture.

The CLI downloads are direct executables (`.exe` on Windows), with MediaCap and FFmpeg linked statically. On Linux, mark the downloaded file executable with `chmod +x MediaCap-cli-linux-x64` or `chmod +x MediaCap-cli-linux-arm64`.

AudioPix GUI downloads are archives containing the application and its runtime files. Extract the whole archive and run `bin/AudioPix.exe` on Windows or `bin/AudioPix` on Linux. Keep the bundled files together.

| Download | Windows | Linux | macOS |
| --- | --- | --- | --- |
| Library, Release shared | [x64][w-rsh-x] · [ARM64][w-rsh-a] | [x64][l-rsh-x] · [ARM64][l-rsh-a] | [Not yet available][latest] |
| Library, Release static | [x64][w-rst-x] · [ARM64][w-rst-a] | [x64][l-rst-x] · [ARM64][l-rst-a] | [Not yet available][latest] |
| Library, Debug shared | [x64][w-dsh-x] · [ARM64][w-dsh-a] | [x64][l-dsh-x] · [ARM64][l-dsh-a] | [Not yet available][latest] |
| CLI | [x64][w-cli-x] · [ARM64][w-cli-a] | [x64][l-cli-x] · [ARM64][l-cli-a] | [Not yet available][latest] |
| AudioPix GUI | [x64][w-gui-x] · [ARM64][w-gui-a] | [x64][l-gui-x] · [ARM64][l-gui-a] | [Not yet available][latest] |

## Requirements

- Windows downloads require a matching x64 or ARM64 Windows system. When using the shared library, keep `MediaCap.dll` beside your application or on its DLL search path.
- Linux downloads target Ubuntu 24.04 and require glibc 2.39 or newer, a compatible GNU C++ runtime, and the X11, ALSA, and PipeWire runtime libraries. Screen and window capture require an accessible X11 display. System and process audio capture require a running PipeWire session.
- AudioPix on Linux also needs xkbcommon, Fontconfig, and OpenGL/EGL runtime libraries. The Slint runtime is bundled in the GUI archive.
- See the [build guide](docs/building.rst) for development dependencies, platform setup, and macOS source builds.

## Building

See the [build instructions](docs/building.rst) for Windows, Linux, and macOS.

See the [AudioPix guide](gui/README.md) for the desktop app.

See the [.NET package guide](bindings/dotnet/README.md) to build and use the C# bindings.

## Documentation

Start with the [documentation index](docs/index.rst), browse the [examples](docs/examples/index.rst), or read the [CLI guide](docs/cli.rst). Run `MediaCap --help` or `MediaCap docs` for terminal help.

## Credits

- [FlexAudio](https://github.com/Studio-Sadola/flexaudio)
- [Screen Capture Lite](https://github.com/smasherprog/screen_capture_lite)
- [CameraCapture](https://github.com/wysaid/CameraCapture)
- [miniaudio](https://miniaud.io/)
- [FFmpeg](https://ffmpeg.org/)

## Contributions

Contributions, bug fixes, and suggestions are welcome. Please follow the [contribution guide](CONTRIBUTING.md).

## License

MediaCap is licensed under the GNU Lesser General Public License, version 3 or later. Third-party libraries retain their own licenses. See the [license](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).

[latest]: https://github.com/still-standing88/media-cap/releases/latest
[w-rsh-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-x64-release-shared.zip
[w-rsh-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-arm64-release-shared.zip
[w-rst-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-x64-release-static.zip
[w-rst-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-arm64-release-static.zip
[w-dsh-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-x64-debug-shared.zip
[w-dsh-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-windows-arm64-debug-shared.zip
[l-rsh-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-x64-release-shared.tar.gz
[l-rsh-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-arm64-release-shared.tar.gz
[l-rst-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-x64-release-static.tar.gz
[l-rst-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-arm64-release-static.tar.gz
[l-dsh-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-x64-debug-shared.tar.gz
[l-dsh-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-library-linux-arm64-debug-shared.tar.gz
[w-cli-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-cli-windows-x64.exe
[w-cli-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-cli-windows-arm64.exe
[l-cli-x]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-cli-linux-x64
[l-cli-a]: https://github.com/still-standing88/media-cap/releases/latest/download/MediaCap-cli-linux-arm64
[w-gui-x]: https://github.com/still-standing88/media-cap/releases/latest/download/AudioPix-windows-x64.zip
[w-gui-a]: https://github.com/still-standing88/media-cap/releases/latest/download/AudioPix-windows-arm64.zip
[l-gui-x]: https://github.com/still-standing88/media-cap/releases/latest/download/AudioPix-linux-x64.tar.gz
[l-gui-a]: https://github.com/still-standing88/media-cap/releases/latest/download/AudioPix-linux-arm64.tar.gz
