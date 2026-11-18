Examples
========

Each example is a separate C++17 program under ``examples/``. Build the
library with examples enabled, then run each executable from the build output.
CMake on Windows places executables in the build directory's ``bin/``
subdirectory. Meson puts examples in ``examples/`` inside its build
directory. CMake on macOS and Linux also uses the build directory's
``examples/`` subdirectory. SCons places executables at the root of its
configuration output directory.

Use the executable path, adding ``.exe`` on Windows. Every example accepts
``--help``. Start with source discovery to obtain the indices or IDs used by
the capture examples.

.. toctree::
   :maxdepth: 2

   discovery
   recording
   frames
   formats-and-errors

The examples share argument parsing, error reporting and the timed and manual
recording helpers in ``examples/common.hpp``. The screenshot and video frame
examples share the BGRA-to-PPM helper in ``examples/image.hpp``.

.. _example-helpers:

Shared helpers
--------------

.. literalinclude:: ../../examples/common.hpp
   :language: cpp
   :caption: examples/common.hpp

.. literalinclude:: ../../examples/image.hpp
   :language: cpp
   :caption: examples/image.hpp
