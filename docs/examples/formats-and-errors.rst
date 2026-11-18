Formats and error handling
==========================

15 Formats
----------

Chooses a container and codec explicitly. The audio choices are WAV, FLAC, Opus, MP3 and M4A. The video choices are MP4, WebM and MKV.

.. code-block:: text

   15_formats <audio|video> <source-index> <format> <output-file>

.. literalinclude:: ../../examples/15_formats.cpp
   :language: cpp
   :caption: examples/15_formats.cpp

16 Error handling
-----------------

Registers a process-wide error listener and starts a deliberately invalid session. It checks the retained session error separately from the callback.

.. code-block:: text

   16_error_handling

.. literalinclude:: ../../examples/16_error_handling.cpp
   :language: cpp
   :caption: examples/16_error_handling.cpp
