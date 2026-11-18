C++ API
=======

Include ``MediaCap.hpp`` and compile as C++17 or newer. The inline C++ layer
uses the C API and provides a movable session plus discovery results with C++
ownership. Function signatures below come from the public headers.

For the C handle API and shared structures, see :doc:`c`.

Session
-------

``MediaCap::Session`` owns its handle and stops capture during destruction. Add sources before starting. A stop clears the source list. Keep callback state alive until the session stops.

.. doxygenclass:: MediaCap::Session
   :members:

Source discovery
----------------

The classic discovery calls return vectors of C-compatible descriptors. Detailed camera discovery and the FlexAudio calls return values that own their strings or arrays.

.. doxygenfunction:: MediaCap::get_monitors

.. doxygenfunction:: MediaCap::get_windows

.. doxygenfunction:: MediaCap::get_camera_devices

.. doxygenfunction:: MediaCap::get_cameras

.. doxygenfunction:: MediaCap::get_audio_devices

.. doxygenfunction:: MediaCap::get_audio_capture_devices

.. doxygenfunction:: MediaCap::get_audio_processes

.. doxygenstruct:: MediaCap::CameraDetail
   :members:

.. doxygenstruct:: MediaCap::AudioCaptureDevice
   :members:

.. doxygenstruct:: MediaCap::AudioProcess
   :members:

FlexAudio configuration
-----------------------

Initialize routing settings with ``default_audio_source_config()`` and inspect ``get_audio_capabilities()`` before adding a routed audio source.

.. doxygenfunction:: MediaCap::default_audio_source_config

.. doxygenfunction:: MediaCap::get_audio_capabilities

.. doxygentypedef:: MediaCap::AudioSourceConfig

.. doxygentypedef:: MediaCap::AudioCapabilities

Screenshots
-----------

The two overloads capture a monitor or window and deliver borrowed BGRA pixels. Use the shared :c:struct:`MediaCap_ScreenshotConfig` for a crop, resize or timeout.

.. doxygenfunction:: MediaCap::capture_screenshot(const Monitor&, MediaCap_ScreenshotCallback, void*, const MediaCap_ScreenshotConfig&)

.. doxygenfunction:: MediaCap::capture_screenshot(const Window&, MediaCap_ScreenshotCallback, void*, const MediaCap_ScreenshotConfig&)

Errors
------

The C and C++ error listener calls update the same process-wide listener. Use the session methods to read failures associated with a particular recording.

.. doxygenfunction:: MediaCap::set_error_callback

.. doxygenfunction:: MediaCap::get_last_error

.. doxygenfunction:: MediaCap::clear_last_error

.. doxygentypedef:: MediaCap::Error

.. doxygentypedef:: MediaCap::ErrorCallback

Shared type aliases
-------------------

These aliases name the structures declared in the C headers. The :doc:`c` page lists their fields, enums and callback types.

.. cpp:type:: MediaCap::Resolution = MediaCap_Resolution

.. cpp:type:: MediaCap::Monitor = MediaCap_Monitor

.. cpp:type:: MediaCap::Window = MediaCap_Window

.. cpp:type:: MediaCap::Camera = MediaCap_Camera

.. cpp:type:: MediaCap::AudioDevice = MediaCap_AudioDevice

.. cpp:type:: MediaCap::CameraCaptureConfig = MediaCap_CameraCaptureConfig

.. cpp:type:: MediaCap::ScreenCaptureConfig = MediaCap_ScreenCaptureConfig

.. cpp:type:: MediaCap::WindowCaptureConfig = MediaCap_WindowCaptureConfig

.. cpp:type:: MediaCap::VideoEncoderConfig = MediaCap_VideoEncoderConfig

.. cpp:type:: MediaCap::AudioEncoderConfig = MediaCap_AudioEncoderConfig

.. cpp:type:: MediaCap::RecordingConfig = MediaCap_RecordingConfig

.. cpp:type:: MediaCap::PackedFrame = std::variant<MediaCap_AudioFrame, MediaCap_VideoFrame>

   A non-owning audio or video frame descriptor. Copying the variant does not
   extend the lifetime of the underlying buffers.
