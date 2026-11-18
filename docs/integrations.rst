Third-party integration
=======================

Text and filenames
------------------

MediaCap uses UTF-8 for public C and C++ text, recording filenames, and CLI
arguments and JSON. Python and .NET convert their strings to UTF-8 when they
call the library. Discovery preserves the spelling of window titles and
replaces malformed display text. Fixed-size fields are truncated at Unicode
character boundaries. FlexAudio IDs are exact UTF-8 strings and classic audio
device IDs are opaque bytes.

Screen and window capture
-------------------------

MediaCap uses Screen Capture Lite to discover monitors and windows and obtain
frames. The platform backends use DXGI desktop duplication and GDI on Windows,
macOS display and window APIs on macOS, and X11 on Linux. An X11 display is
required for screen and window capture on Linux. Camera and audio capture use
their own backends.

Platform requirements
---------------------

Run ``MediaCap capabilities requirements`` or add ``--json`` to read a
machine-readable feature list. Applications can query the same data with
``MediaCap_GetFeatureRequirements()``, C++ ``get_feature_requirements()``,
Python ``get_feature_requirements()`` or .NET
``SourceDiscovery.GetFeatureRequirements()``.

The query reports which backends were built, their known minimum OS versions,
and prerequisites such as X11, PipeWire and OS privacy permissions. On macOS,
the FlexAudio system, process, mix and exclusion routes require macOS 14.4 or
later. On Windows, process capture and exclusion require build 20348 or later.
Linux system and process audio use PipeWire. An absent minimum version means
that the query has no feature-specific version floor to report.

Use ``MediaCap capabilities audio --details`` for runtime audio availability.
Device access and consent are checked during capture startup. For binary
deployment and system library requirements, see :doc:`building`.

Camera capture
--------------

CameraCapture provides camera discovery, format queries and frame delivery. The
bundled implementation uses DirectShow or Media Foundation on Windows,
AVFoundation on macOS and V4L2 on Linux. Use
``MediaCap::get_camera_devices()`` to list camera identities and
``MediaCap::get_cameras()`` to inspect supported formats. Querying detailed
capabilities can open a device, so a busy or inaccessible camera may yield an
incomplete result.

Audio capture
-------------

miniaudio supplies the classic audio-device interface. Its device IDs are opaque
and are used with the classic source-add functions. A classic Windows output
source uses WASAPI loopback when the discovered device supports it.

FlexAudio supplies the optional microphone, system-output and process-audio
interface. It is enabled by default and built from the vendored Rust sources.
On Linux it uses ALSA for microphone capture and PipeWire for system and process
audio. Platform support and permissions affect the available routes. Query
``MediaCap_GetAudioCapabilities()`` or ``MediaCap::get_audio_capabilities()``
before enabling a routing option in your application.

FlexAudio device IDs come from its own discovery API. Use them with
``MediaCap_AudioSourceConfig`` or ``MediaCap::AudioSourceConfig``. For
process capture, use a nonzero PID from process discovery and check the reported
PID scope. The scope describes whether a route matches one process or a process
tree.

Encoding and containers
-----------------------

MediaCap links the FFmpeg development libraries. ``libavcodec`` encodes media,
``libavformat`` writes containers, ``libswscale`` converts video frames,
and ``libswresample`` converts audio sample rates and layouts.
``libavutil`` supplies shared media types and helpers.

The library exposes H.264, VP9 and AV1 video encoding and AAC, Opus, Vorbis, FLAC,
MP3 and PCM audio encoding. An output container must support the selected codecs.
The following are combinations used by the examples and integration tests.

.. list-table::
   :header-rows: 1
   :widths: 24 36 40

   * - Container
     - Video example
     - Audio example
   * - MP4
     - H.264
     - AAC
   * - Matroska (MKV)
     - H.264 or AV1
     - PCM
   * - WebM
     - VP9
     - Opus
   * - M4A
     - Audio only
     - AAC
   * - WAV, FLAC, MP3
     - Audio only
     - PCM, FLAC, or MP3 respectively
   * - Ogg Opus
     - Audio only
     - Opus

Choose ``MEDIACAP_CONTAINER_DEFAULT`` to infer the container from the output
extension. An explicit container takes precedence over the extension. The CLI
uses the same preflight as the library, including during ``--dry-run``, to
report audio and video codec or container conflicts before opening devices or
files. Applications can call ``MediaCap_ValidateRecordingFormat()``,
Python's ``validate_recording_format()``, or .NET's ``RecordingConfig.ValidateFormat()``.
Selected devices are checked when capture starts. Audio sample rates
can differ between capture and encoding when the pipeline can resample them.
The build checks the linked FFmpeg configuration and requires the encoders used
by MediaCap. See :doc:`building` for dependency installation.
