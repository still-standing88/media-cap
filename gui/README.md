# AudioPix

AudioPix is a Slint desktop recorder for MediaCap. Build it with the optional
`--gui` switch. See the [build guide](../docs/building.rst#build-audiopix) for the
external Slint SDK and the CMake, Meson, and SCons options.

- In **Session**, add or select a session and choose **Add source**. Sources
  include cameras, monitors, audio inputs, output loopback, application windows,
  and process audio. Each session can record one video source and mix multiple
  audio sources.
- Right-click a session or source to open its context menu. The Menu key opens
  it from the focused list. The Session actions and Source actions buttons
  also open those menus. Edit changes
  capture settings for the selected device. Use Preview for live video or an
  audio peak meter.
- Use **Start**, **Pause** or **Resume**, and **Stop and save** to record.
  **Cancel recording** discards the current recording. AudioPix writes a
  timestamped Matroska file in the output directory selected in **Preferences**.
- Use **Audio** and **Video** for encoder settings. **Keyboard Shortcuts**
  accepts combinations such as `Ctrl+Shift+F9` for start, pause/resume, and
  stop. A blank shortcut disables it.
- The system tray menu can open AudioPix, control recording, or quit.
  Preferences can keep the app running in the tray when its window closes.

Sessions, preferences, and shortcuts are stored in one UTF-8 `settings.cfg`
file. The directory is `%APPDATA%\AudioPix` on Windows,
`~/Library/Application Support/AudioPix` on macOS, and
`${XDG_CONFIG_HOME:-~/.config}/AudioPix` on Linux. Window handles and process
IDs come from the current desktop session. If a saved target has closed or
restarted, remove the stale source and add the current one.

Capture availability depends on MediaCap and OS permissions. On macOS, grant
the applicable capture permissions and Input Monitoring for global shortcuts.
On Linux, MediaCap screen and window capture uses X11, keyboard shortcuts need
read access to keyboard input devices, and the tray needs a desktop with
StatusNotifierItem support. Process capture requires an enabled and supported
FlexAudio backend.
