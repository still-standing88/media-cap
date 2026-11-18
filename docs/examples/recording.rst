Recording
=========

Examples 03 through 09 stop and finalize the recording when you press Enter.
Use an output filename you are willing to overwrite.

03 Audio input recording
------------------------

Records a classic microphone or other input as a mono PCM WAV file. Use an audio input index from source discovery.

.. code-block:: text

   03_audio_input <audio-device-index> <output.wav>

.. literalinclude:: ../../examples/03_audio_input.cpp
   :language: cpp
   :caption: examples/03_audio_input.cpp

04 Audio output recording
-------------------------

Records system output through FlexAudio. Omit the device ID to use the default output, or pass a FlexAudio device ID from source discovery.

.. code-block:: text

   04_audio_output <output.wav> [device-id]

.. literalinclude:: ../../examples/04_audio_output.cpp
   :language: cpp
   :caption: examples/04_audio_output.cpp

05 Audio process recording
--------------------------

Records audio for a selected process through FlexAudio. Choose a nonzero PID and a platform that reports process audio support.

.. code-block:: text

   05_audio_process <pid> <output.wav>

.. literalinclude:: ../../examples/05_audio_process.cpp
   :language: cpp
   :caption: examples/05_audio_process.cpp

06 Window recording
-------------------

Records a visible top-level window to a Matroska file. Use a window index from source discovery.

.. code-block:: text

   06_window_recording <window-index> <output.mkv>

.. literalinclude:: ../../examples/06_window_recording.cpp
   :language: cpp
   :caption: examples/06_window_recording.cpp

07 Monitor recording
--------------------

Records a complete monitor to a Matroska file. The encoder uses the dimensions reported for that monitor.

.. code-block:: text

   07_monitor_recording <monitor-index> <output.mkv>

.. literalinclude:: ../../examples/07_monitor_recording.cpp
   :language: cpp
   :caption: examples/07_monitor_recording.cpp

08 Camera recording
-------------------

Requests a 640 × 480 BGRA camera stream and writes H.264 in a Matroska container. Choose a device that can supply a suitable mode.

.. code-block:: text

   08_camera_recording <camera-index> <output.mkv>

.. literalinclude:: ../../examples/08_camera_recording.cpp
   :language: cpp
   :caption: examples/08_camera_recording.cpp

09 Audio and video session
--------------------------

Records one monitor and one classic audio input in a single Matroska file. The session supplies a common timeline for the tracks.

.. code-block:: text

   09_audio_video_session <monitor-index> <audio-input-index> <output.mkv>

.. literalinclude:: ../../examples/09_audio_video_session.cpp
   :language: cpp
   :caption: examples/09_audio_video_session.cpp

10 Timed session recording
--------------------------

Records a monitor to MP4 and stops automatically after the requested duration. The duration must be positive and begins after capture preparation.

.. code-block:: text

   10_timed_session <monitor-index> <output.mp4> <seconds>

.. literalinclude:: ../../examples/10_timed_session.cpp
   :language: cpp
   :caption: examples/10_timed_session.cpp
