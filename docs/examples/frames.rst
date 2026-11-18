Screenshots and raw frames
==========================

11 Screenshot
-------------

Captures a single monitor or window image and writes it as a binary PPM file. The optional crop uses source-relative coordinates in pixels. Specify both output dimensions to resize the crop. Use zero for both crop dimensions to select the entire source, or zero for both output dimensions to keep the crop size.

.. code-block:: text

   11_screenshot <monitor|window> <index> <output.ppm> [x y width height output-width output-height]

.. literalinclude:: ../../examples/11_screenshot.cpp
   :language: cpp
   :caption: examples/11_screenshot.cpp

12 Frame callbacks
------------------

Starts capture-only mode for a monitor and a microphone. Callbacks count video frames and audio blocks by source index. Keep callback state alive until the session stops.

.. code-block:: text

   12_callbacks <monitor-index> <audio-input-index>

.. literalinclude:: ../../examples/12_callbacks.cpp
   :language: cpp
   :caption: examples/12_callbacks.cpp

13 Video frames
---------------

Copies one borrowed BGRA video frame before the callback returns. The shared image helper uses the frame's row stride and orientation when converting it to RGB for the PPM file.

.. code-block:: text

   13_video_frames <monitor-index> <output.ppm>

.. literalinclude:: ../../examples/13_video_frames.cpp
   :language: cpp
   :caption: examples/13_video_frames.cpp

14 Audio samples
----------------

Reads interleaved float PCM from an audio callback. The example counts sample frames and computes peak amplitude from the sample values.

.. code-block:: text

   14_audio_samples <audio-input-index>

.. literalinclude:: ../../examples/14_audio_samples.cpp
   :language: cpp
   :caption: examples/14_audio_samples.cpp
