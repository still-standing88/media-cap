Discovery and device information
================================

01 Source discovery and device enumeration
------------------------------------------

Lists monitor, window, camera and classic audio indices. If FlexAudio is enabled, it also lists its device IDs and audio process IDs.

.. code-block:: text

   01_source_discovery

.. literalinclude:: ../../examples/01_source_discovery.cpp
   :language: cpp
   :caption: examples/01_source_discovery.cpp

02 Source and device information
--------------------------------

Prints geometry and scaling for monitors and windows, supported camera formats, classic audio device roles and FlexAudio capabilities.

.. code-block:: text

   02_source_info

.. literalinclude:: ../../examples/02_source_info.cpp
   :language: cpp
   :caption: examples/02_source_info.cpp
