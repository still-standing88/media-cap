# AudioPix implementation plan

- [x] Inspect the MediaCap capture API, build frontends, and existing binding work.
- [x] Build five Slint tabs with keyboard-operable session and source context menus and persistent settings.
- [x] Connect source discovery, live preview, recording controls, global shortcuts, and the system tray.
- [x] Add optional AudioPix builds to CMake, Meson, SCons, and the shared build driver.
- [x] Review UTF-8 handling and native lifetimes in the Python and .NET bindings and run their smoke tests.
- [x] Build and exercise AudioPix on Windows and document the platform requirements.

AudioPix uses the MediaCap C++ API. Each recording uses one video source and
can mix multiple audio sources. Capture operations run off the UI thread,
and the UI takes an owned copy of preview data. The config parser stores
sessions and hotkeys in a per-user .cfg file. Slint comes from an external
SDK or a pinned dependency fetched into the CMake build directory.

The existing bindings use pybind11 for CPython and P/Invoke with SafeHandle
for .NET. Both use the MediaCap C ABI, keep UTF-8 buffers live for the
duration of each native call, and copy callback frames before returning
from the native callback.

Windows verification covered CMake and SCons with shared MediaCap, Meson
with static MediaCap, keyboard access to the session menu and source dialog,
Unicode settings and output paths, window preview, recording, pause/resume,
save, and cancel. Python lifecycle tests and .NET package checks passed.
Three Python tests skipped because that test invocation lacked display access.
Linux and macOS builds and end-to-end audio and shortcut capture remain
unverified.
