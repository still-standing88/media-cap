C API
=====

Include ``MediaCap_c.h`` to use the C API. It includes the shared types and
FlexAudio declarations. Functions and types on this page are generated from
the public headers.

For C++ ownership and convenience wrappers, see :doc:`cpp`.

Session lifecycle
-----------------

Create one handle per session and destroy it after use. Serialize operations that change a session. Polling status or reading its error record is safe from another thread while the handle remains valid.

.. doxygenfunction:: MediaCap_Session_Create

.. doxygenfunction:: MediaCap_Session_Destroy

.. doxygenfunction:: MediaCap_Session_GetId

Device discovery
----------------

Classic discovery fills a caller-owned array. Query the count first, then call again with the array and its capacity. Free the nested arrays returned by detailed camera discovery with ``MediaCap_FreeCameraInfoContents()``.

.. doxygenfunction:: MediaCap_GetMonitors

.. doxygenfunction:: MediaCap_GetWindows

.. doxygenfunction:: MediaCap_GetCameraDevices

.. doxygenfunction:: MediaCap_GetCameras

.. doxygenfunction:: MediaCap_FreeCameraInfoContents

.. doxygenfunction:: MediaCap_GetAudioDevices

Source configuration
--------------------

Add sources before starting capture or recording. After a stop, add sources again before starting another session on the same handle.

.. doxygenfunction:: MediaCap_Session_AddMonitorSource

.. doxygenfunction:: MediaCap_Session_AddWindowSource

.. doxygenfunction:: MediaCap_Session_AddCameraSource

.. doxygenfunction:: MediaCap_Session_AddAudioSource

.. doxygenfunction:: MediaCap_Session_AddAudioCaptureSource

Recording control
-----------------

Record one video source and any configured audio sources into a single file. MediaCap mixes the audio into one track. Start waits for the sources to become ready before opening the file.

.. doxygenfunction:: MediaCap_Session_StartRecording

.. doxygenfunction:: MediaCap_Session_StartRecordingForDuration

.. doxygenfunction:: MediaCap_Session_StopRecording

.. doxygenfunction:: MediaCap_Session_SetPaused

.. doxygenfunction:: MediaCap_Session_IsRecording

.. doxygenfunction:: MediaCap_Session_IsPaused

Raw frame delivery
------------------

Register frame callbacks before starting. Callbacks can run concurrently and receive borrowed read-only data. Copy any needed bytes before returning. Stop the session before freeing the callback state.

.. doxygenfunction:: MediaCap_Session_SetFrameCallbacks

.. doxygenfunction:: MediaCap_Session_StartCapture

.. doxygenfunction:: MediaCap_Session_StopCapture

.. doxygenfunction:: MediaCap_Session_IsCapturing

.. c:type:: void (*MediaCap_AudioFrameCallback)(size_t source_index, MediaCap_SourceType source_type, const MediaCap_AudioFrameView *frame, void *user_data)

   Receives an audio block with its zero-based source index and type. The
   frame and sample data are borrowed for the duration of the callback.

.. c:type:: void (*MediaCap_VideoFrameCallback)(size_t source_index, MediaCap_SourceType source_type, const MediaCap_VideoFrameView *frame, void *user_data)

   Receives a video frame with its zero-based source index and type. The
   frame and image data are borrowed for the duration of the callback.

.. doxygenstruct:: MediaCap_AudioFrameView
   :members:

.. doxygenstruct:: MediaCap_VideoFrameView
   :members:

Screenshots
-----------

A screenshot call waits for one image and invokes the callback on the calling thread. The crop is relative to the source. The configuration can also set the output dimensions and first-frame timeout.

.. doxygenfunction:: MediaCap_CaptureMonitorScreenshot

.. doxygenfunction:: MediaCap_CaptureWindowScreenshot

.. c:type:: void (*MediaCap_ScreenshotCallback)(const MediaCap_VideoFrameView *frame, void *user_data)

   Receives a top-down BGRA screenshot on the calling thread. Copy the
   image during the callback to retain it.

.. doxygenstruct:: MediaCap_ScreenshotConfig
   :members:

FlexAudio routing and discovery
-------------------------------

FlexAudio discovery allocates result arrays and strings. Release each inventory with its matching free function. Query capabilities before choosing a route.

.. doxygenfunction:: MediaCap_DefaultAudioSourceConfig

.. doxygenfunction:: MediaCap_GetAudioCapabilities

.. doxygenfunction:: MediaCap_GetAudioCaptureDevices

.. doxygenfunction:: MediaCap_FreeAudioCaptureDevices

.. doxygenfunction:: MediaCap_GetAudioProcesses

.. doxygenfunction:: MediaCap_FreeAudioProcesses

.. doxygenstruct:: MediaCap_AudioSourceConfig
   :members:

.. doxygenstruct:: MediaCap_AudioCaptureDevice
   :members:

.. doxygenstruct:: MediaCap_AudioProcess
   :members:

.. doxygenstruct:: MediaCap_AudioCapabilities
   :members:

.. doxygenenum:: MediaCap_AudioCaptureKind

.. doxygenenum:: MediaCap_ProcessAudioMode

.. doxygenenum:: MediaCap_AudioAvailability

.. doxygenenum:: MediaCap_AudioPidScope

Error reporting
---------------

Use the session error record for recording and capture failures. Use the global record for standalone operations such as discovery and screenshots.

.. doxygenfunction:: MediaCap_SetErrorCallback

.. doxygenfunction:: MediaCap_GetLastError

.. doxygenfunction:: MediaCap_ClearLastError

.. doxygenfunction:: MediaCap_Session_HasError

.. doxygenfunction:: MediaCap_Session_GetError

.. c:type:: void (*MediaCap_ErrorCallback)(const MediaCap_Error *error, void *user_data)

   Receives a borrowed diagnostic on the reporting thread. See
   :c:func:`MediaCap_SetErrorCallback` for concurrency and lifetime rules.

.. doxygenstruct:: MediaCap_Error
   :members:

.. doxygenenum:: MediaCap_ErrorCode

.. doxygenenum:: MediaCap_ErrorComponent

.. doxygenenum:: MediaCap_LogLevel

Shared types
------------

These types are declared in ``MediaCap_types.h`` and used by both APIs.

.. doxygentypedef:: MediaCap_Session_Handle

.. doxygenstruct:: MediaCap_Resolution
   :members:

.. doxygenstruct:: MediaCap_Monitor
   :members:

.. doxygenstruct:: MediaCap_Window
   :members:

.. doxygenstruct:: MediaCap_Camera
   :members:

.. doxygenstruct:: MediaCap_CameraInfo
   :members:

.. doxygenstruct:: MediaCap_AudioDevice
   :members:

.. doxygenstruct:: MediaCap_CameraCaptureConfig
   :members:

.. doxygenstruct:: MediaCap_ScreenCaptureConfig
   :members:

.. doxygenstruct:: MediaCap_WindowCaptureConfig
   :members:

.. doxygenstruct:: MediaCap_AudioFormat
   :members:

.. doxygenstruct:: MediaCap_VideoFrame
   :members:

.. doxygenstruct:: MediaCap_AudioFrame
   :members:

.. doxygenenum:: MediaCap_SourceType

.. doxygenenum:: MediaCap_PixelFormat

.. doxygenenum:: MediaCap_FrameOrientation

.. doxygenenum:: MediaCap_AudioDeviceType

.. doxygenenum:: MediaCap_SampleFormat

Output configuration
--------------------

Set the encoder configuration for each kind of source in the session. The output path and encoder settings are copied when recording starts.

.. doxygenstruct:: MediaCap_RecordingConfig
   :members:

.. doxygenstruct:: MediaCap_VideoEncoderConfig
   :members:

.. doxygenstruct:: MediaCap_AudioEncoderConfig
   :members:

.. doxygenenum:: MediaCap_ContainerFormat

.. doxygenenum:: MediaCap_VideoCodec

.. doxygenenum:: MediaCap_AudioCodec

Build definitions
-----------------

.. doxygendefine:: MEDIACAP_API

.. doxygendefine:: MEDIACAP_MA_DEVICE_ID_SIZE
