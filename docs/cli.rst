Command-line tool
=================

The ``MediaCap`` executable discovers capture sources, records media, saves
screenshots, and exposes raw video frames and audio samples. Build the library
with ``--cli`` as described in :doc:`building`. On Windows, run
``.\MediaCap.exe`` from the installation directory or put its ``bin`` directory
on ``PATH``.

Find a capture source
---------------------

List a single category to see the IDs and names you can pass to recording,
screenshot, or inspection commands. Use ``--name`` to filter a list without
renumbering the results. ``--details`` adds fields to the list. ``info``
queries one source and displays its available details.

.. code-block:: console

   MediaCap list cameras
   MediaCap info camera 0
   MediaCap list audio processes --active
   MediaCap info audio-process 1234
   MediaCap capabilities audio --details
   MediaCap --json capabilities requirements
   MediaCap capabilities formats

Replace example IDs, window handles, and process IDs with values from your
source lists. Audio input and output use separate ID sequences. Use an ID from
the matching list or the full device ID shown in its detailed output. Prefix a
full device ID with ``id:`` to force a literal match, for example ``id:100``.
Camera capability queries can open the camera briefly to check supported modes.

Record media and take screenshots
----------------------------------

Use ``record`` with a source and an output file. Choose an explicit container
or let MediaCap infer it from the file extension. Use ``--dry-run`` to check
recording settings before opening a device or writing a file. By default, an
existing output file is protected from replacement.

.. code-block:: console

   MediaCap record screen --audio output -o desktop.mp4 --duration 10
   MediaCap record audio --audio input:0 -o voice.wav --duration 10
   MediaCap record window --title "My Window" -o window.mp4 --duration 10
   MediaCap record camera --camera 0 -o camera.mp4 --duration 10

Video recording takes one video source. Repeat ``--audio`` to add sources to
the mixed audio track. Use ``MediaCap help audio-options`` for per-source gain,
format, and routing settings. Use ``MediaCap help controls`` for interactive and
stdin commands. Ctrl+C stops a running recording and saves its output.

Use ``screenshot`` to write one 24-bit BMP image from a monitor or window. The crop
coordinates are relative to the selected source. ``--size`` scales the
result after cropping. ``--timeout`` sets the time allowed to receive the first
frame in milliseconds. Its default is 10 seconds.

.. code-block:: console

   MediaCap screenshot screen --monitor 0 -o desktop.bmp
   MediaCap screenshot window --window 0x12345 -o window.bmp
   MediaCap screenshot screen --region 0,0,800,600 --size 400x300 -o crop.bmp

Capture raw frames and samples
------------------------------

Use ``capture`` to receive session data without an encoded recording. Pass
``--video-frames`` or ``--audio-samples`` to save the corresponding stream.
Without an output file, ``capture`` reports frame or sample totals. Use
``status`` and ``stop`` to control a raw capture session. These output options
also work with ``record``, and frame delivery continues during a recording
pause. Files keep the first frame or audio block from each source during
startup, then receive subsequent data after the session starts. Frame and
sample totals count all received data, including startup data omitted from
the file.

.. code-block:: console

   MediaCap capture screen --duration 2 --video-frames screen.frames
   MediaCap capture audio --audio input --duration 2 --audio-samples mic.frames

Each sink starts with an ASCII ``MediaCap frames 1`` line. It then repeats a
UTF-8 JSON metadata line and exactly ``payload_bytes`` bytes of media data,
until EOF. The next metadata line starts immediately after the payload. Video
planes are stored row by row without stride padding. Audio samples are
interleaved with little-endian multibyte samples.

Every metadata object includes ``type`` (``video`` or ``audio``),
``source_index`` (the zero-based index within the session, with video first),
``source_type`` (the numeric ``MediaCap_SourceType`` value), ``timestamp_ns``
(the monotonic host timestamp in nanoseconds), and
``payload_bytes``. Video also includes ``frame_index``, ``width``, ``height``,
``pixel_format``, ``orientation`` (``top-down`` or ``bottom-up``), and a
``planes`` array with ``row_bytes`` and ``rows`` for each plane in payload
order. Audio also includes ``sample_frames``, ``channels``, ``sample_rate``,
and ``sample_format`` (``u8``, ``s16le``, ``s32le``, ``f32le``, or ``f64le``).

Use JSON in scripts
-------------------

Use ``--json`` to request machine-readable results on stdout. A query writes
its result as JSON and a session writes one event object per line. In JSON
mode, errors are objects on stdout with ``"event": "error"``. Warnings and
``--verbose`` diagnostics go to stderr. In text mode, errors also go to stderr.
Check the process exit status. ``0`` means success, ``1`` means runtime failure,
and ``2`` means invalid input or settings.

Session events use the ``event`` field to identify the event. ``status`` and
``finished`` include the totals ``video_frames``, ``audio_blocks``, and
``sample_frames``. A ``--dry-run`` writes a ``plan`` event.

To pass a JSON request, save a UTF-8 file and run ``MediaCap run request.json``.
Use ``MediaCap run -`` to read the request from stdin. A request read from stdin
cannot use ``--stdin-control``. Requests cannot invoke ``run`` recursively.

.. code-block:: json

   {
     "command": ["record", "audio"],
     "options": {
       "audio": ["input"],
       "output": "voice.wav",
       "duration": 10,
       "dry-run": true
     }
   }

The ``command`` array names the command and its subcommands. ``options`` uses
long option names, with optional leading ``--``. Set flag options to booleans.
Use strings or numbers for values and arrays to repeat an option. An optional
``arguments`` array supplies positional strings or numbers. JSON requests
produce JSON output by default. Set ``"json": false`` inside ``options`` to
request terminal text, or pass ``--json`` to ``run`` to force JSON output.

Generate command documentation
------------------------------

Use ``--help`` for a command's usage and example, or ``--help-all`` for a
reference covering all of its subcommands. Use ``help`` to open a named guide.
The executable prints the command reference as plain text by default. Use
``--format rst`` or a ``.rst`` output filename to export reStructuredText.

.. code-block:: console

   MediaCap record camera --help
   MediaCap --help-all
   MediaCap help audio
   MediaCap docs
   MediaCap docs --output command-reference.txt
   MediaCap docs --output command-reference.rst

To include the generated reference in the HTML documentation, pass the path to
your built MediaCap executable with ``--cli``. See :doc:`building` for the
documentation tool setup.

.. code-block:: console

   python scripts/build_api_docs.py --cli /path/to/MediaCap
