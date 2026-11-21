Building
========

Run the build scripts from a source checkout. They select a build system,
check the compiler and dependencies, and build MediaCap. The default is a
shared Release build using CMake with the examples and FlexAudio enabled.
The CLI, AudioPix, Python bindings, and tests are opt-in.

Common requirements
-------------------

- A C++17 compiler and the native SDK for your operating system.
- Python 3.9 or newer and one build system. Use CMake 3.20 or newer with Ninja, Meson 1.3
  or newer with Ninja, or SCons.
- FFmpeg 5.1 or newer with the required headers, static libraries and encoders.
  The dependency manifest at ``dependencies/vcpkg.json`` specifies the required
  FFmpeg features.
- Rust and Cargo 1.85 or newer when building FlexAudio, which is enabled by
  default.

With a bootstrapped vcpkg checkout, install the dependency manifest from the
MediaCap repository root:

.. code-block:: console

   vcpkg install --x-manifest-root=dependencies --x-install-root=<vcpkg-root>/installed --triplet=<triplet>

Replace ``<vcpkg-root>`` and ``<triplet>`` with the actual location and target
triplet. Put vcpkg on ``PATH`` or invoke its executable by full path. Pass
``--vcpkg-root <vcpkg-root>`` to the build script or set ``VCPKG_ROOT``. Use
``--triplet <triplet>`` to select the same triplet explicitly.

If you already have a compatible FFmpeg build, pass
``--ffmpeg-include-dir <path>`` and ``--ffmpeg-lib-dir <path>`` to use those
directories. Set ``--ffmpeg-debug-lib-dir <path>`` when the Debug archives are
installed separately. The FFmpeg executable alone is insufficient.

Windows
-------

Install the Visual Studio Build Tools **Desktop development with C++**
workload, including MSVC and a Windows SDK. The wrapper locates the tools
with ``vswhere`` or reuses an active Visual Studio developer shell for the
target architecture.

The vcpkg triplet must use the static C runtime. Use ``x64-windows-static``
for x64 or ``arm64-windows-static`` for ARM64. Debug links against ``/MTd``
and Release against ``/MT``. Select FFmpeg archives built with the same runtime
as MediaCap.

When installing the pinned dependency manifest on Windows, add
``--overlay-ports=dependencies/windows-ports`` to the ``vcpkg install`` command.
This applies dav1d's upstream preprocessor compatibility fix for MSVC 19.52
while retaining the pinned codec versions. The Windows workflow includes
this option automatically.

.. code-block:: powershell

   .\build.ps1 --frontend cmake --release --shared --cli

Or run the batch wrapper from Developer Command Prompt.

.. code-block:: bat

   build.bat --frontend cmake --release --shared --cli

macOS
-----

Install the Xcode Command Line Tools or select an Xcode installation with
``xcode-select``. The macOS SDK provides the system frameworks used for
display, window, camera and audio capture. Use ``arm64-osx`` for Apple
silicon or ``x64-osx`` for Intel.

.. code-block:: bash

   bash build.sh --frontend meson --release --shared --cli

Grant the application the relevant macOS privacy permissions before running
examples that capture the display, camera or microphone.

Linux
-----

Install development packages for X11, Xext, XTest, Xfixes and Xinerama. With
FlexAudio enabled, install the ALSA and PipeWire development packages and
the Clang libraries used by the Rust bindings. The package names on
Debian-derived distributions are shown below.

.. code-block:: bash

   sudo apt-get install build-essential pkg-config clang libclang-dev \
     libx11-dev libxext-dev libxtst-dev libxfixes-dev libxinerama-dev \
     libasound2-dev libpipewire-0.3-dev

Use ``x64-linux`` for x64 or ``arm64-linux`` for ARM64. Install the
toolchain for the selected build system before running the wrapper.

.. code-block:: bash

   bash build.sh --frontend scons --release --static --cli

Run screen and window examples against an accessible X11 display. Camera
discovery uses V4L2 devices and requires permission to access them. System and
process audio through FlexAudio require a running PipeWire session.

Build options
-------------

The three wrappers accept the same arguments. Run the wrapper with
``--help`` to see all options. Add ``--check-only`` to run dependency checks
without building. You can also run the check directly.

.. code-block:: console

   python scripts/check_dependencies.py --frontend cmake --release --shared

.. list-table::
   :header-rows: 1
   :widths: 24 25 25 26

   * - Selection
     - CMake
     - Meson
     - SCons
   * - Static or shared
     - ``-DBUILD_SHARED_LIBS=OFF`` or ``ON``
     - ``--default-library=static`` or ``shared``
     - ``shared=0`` or ``1``
   * - Debug or Release
     - ``-DCMAKE_BUILD_TYPE=Debug`` or ``Release``
     - ``--buildtype=debug`` or ``release``
     - ``debug=1`` or ``0``
   * - Build CLI
     - ``-DMEDIACAP_BUILD_CLI=ON``
     - ``-Dbuild_cli=true``
     - ``cli=1``
   * - Build AudioPix
     - ``-DMEDIACAP_BUILD_GUI=ON``
     - ``-Dbuild_gui=true``
     - ``gui=1``
   * - Build Python bindings
     - ``-DMEDIACAP_BUILD_PYTHON=ON``
     - ``-Dbuild_python=true``
     - ``python=1``
   * - Build examples
     - ``-DMEDIACAP_BUILD_EXAMPLES=ON``
     - ``-Dbuild_examples=true``
     - ``examples=1``
   * - Build tests
     - ``-DMEDIACAP_BUILD_TESTS=ON``
     - ``-Dbuild_tests=true``
     - ``tests=1``
   * - Disable FlexAudio
     - ``-DMEDIACAP_FLEXAUDIO=OFF``
     - ``-Dflexaudio=disabled``
     - ``flexaudio=0``

The wrappers select these options with ``--static`` or ``--shared``,
``--debug`` or ``--release``, ``--cli``, ``--gui``, ``--python``, ``--no-examples``, ``--tests``,
and ``--no-flexaudio``. Use ``--offline`` to require cached Cargo dependencies,
``--jobs N`` to set build parallelism, and ``--install-prefix <path>`` to
install after building.

Default CMake and Meson build directories use
``build/<frontend>-<configuration>-<linkage>``. SCons uses
``build-scons/<configuration>-<linkage>``. Use ``--build-dir <path>`` to
select a different output location.

Build AudioPix
--------------

AudioPix requires a C++20 compiler and Slint C++ 1.17 or newer with desktop,
accessibility, and system tray support. Download the SDK matching your
platform and architecture from the `Slint releases`_ and install or extract
it outside the source checkout. Point ``--slint-root`` or ``SLINT_ROOT`` at the
SDK prefix containing ``bin``, ``include``, and ``lib``.

.. code-block:: console

   python scripts/build_library.py --gui --shared --slint-root <slint-sdk>
   python scripts/build_library.py --frontend meson --gui --static --slint-root <slint-sdk>
   python scripts/build_library.py --frontend scons --gui --shared --slint-root <slint-sdk>

For direct builds use ``-DSlint_ROOT=<slint-sdk>`` with CMake,
``-Dslint_root=<slint-sdk>`` with Meson, or ``slint_root=<slint-sdk>`` with SCons.
CMake accepts ``Slint_DIR`` and ``CMAKE_PREFIX_PATH`` as well. If CMake cannot
find an SDK, it fetches the pinned Slint 1.17.0 source into the build directory.
That path needs CMake 3.21+, Git, Rust 1.92+, and the selected Slint backend's
development dependencies. Set ``-DMEDIACAP_FETCH_SLINT=OFF`` to require a local
SDK. Meson and SCons use the shared-library SDK layout, including when you
select static MediaCap. Static and shared here describe the MediaCap linkage.

Run ``AudioPix`` from the build output or install it with
``--install-prefix <path>`` and run it from the installed ``bin`` directory.
Keep the Slint SDK available at the build-time location on Linux and macOS
when using its shared library with Meson or SCons, and on macOS with CMake.
CMake installs the Slint
runtime beside the executable on Windows and Linux, and includes the MSVC
runtime DLLs on Windows. CMake's installed Linux application locates its
bundled Slint runtime relative to the executable. Linux also requires the
xkbcommon development package.
The prebuilt Slint SDK also needs the libinput runtime (``libinput10`` on Ubuntu).

The manually dispatched ``Publish MediaCap release`` workflow builds Windows
and Linux packages for x64 and ARM64 in the public ``still-standing88/media-cap``
repository. It publishes Release shared, Release static, and Debug shared
library archives, direct CLI executables, and AudioPix GUI archives. The CLI
and GUI link MediaCap and FFmpeg statically; GUI archives also include the
Slint runtime and its notices. Extract the complete GUI archive and launch
``bin/AudioPix.exe`` on Windows or ``bin/AudioPix`` on Linux. Linux CLI downloads
need ``chmod +x`` before use. The workflow checks that the README's download
links match the generated release asset names before compiling.

.. _Slint releases: https://github.com/slint-ui/slint/releases

Build and install the Python bindings
-------------------------------------

Use a CPython interpreter that matches the native build architecture.
Install pybind11 into that interpreter's environment and select your build
frontend. Pass ``--python-executable <path>`` to use a different interpreter.

.. code-block:: console

   python -m pip install "pybind11>=3"
   python scripts/build_library.py --frontend cmake --python --no-examples --install-prefix stage

Use ``meson`` or ``scons`` in place of ``cmake`` to stage the package with
another frontend. Run ``python bindings/python/python_config.py platlib`` to find
the package directory relative to ``stage``. Add that directory to
``PYTHONPATH`` and verify that Python loads the staged package.

.. code-block:: console

   python -c "import mediacap; print(mediacap.__file__)"

To install directly into a virtual environment, run ``python -m pip install .``
from the repository root. This builds a wheel with CMake. Set ``VCPKG_ROOT``
and put Cargo on ``PATH`` for the default FlexAudio-enabled build. On Windows,
run the wheel build from a Visual Studio Developer shell for the selected
Python architecture.

Using the installed library
----------------------------

Windows installations provide CMake package metadata. Set ``CMAKE_PREFIX_PATH``
to the installation prefix and link the imported target.

.. code-block:: cmake

   find_package(MediaCap CONFIG REQUIRED)
   target_link_libraries(my_app PRIVATE MediaCap::MediaCap)

Within the MediaCap source tree, the same target is available after adding
the MediaCap project with ``add_subdirectory()``. Include ``MediaCap.hpp``
for C++ or ``MediaCap_c.h`` for C.

Building the documentation
--------------------------

Documentation has an independent build. The setup script uses Python 3.11
or newer to install the pinned documentation tools in a virtual environment.
It uses Doxygen 1.15 or newer from the system or downloads a verified
portable release on Windows x64, Linux x64, and macOS Intel or Apple silicon.
On other architectures, install Doxygen and pass its path with ``--doxygen``.

.. code-block:: console

   python scripts/setup_docs.py
   python scripts/build_api_docs.py

Run the same commands with ``python3`` if that is your Python launcher. Open
``build-api-docs/html/index.html`` after a successful build. Doxygen reads
the public headers and writes XML. Breathe renders that XML in the Sphinx
site. The library's build and test targets have no documentation dependency.
