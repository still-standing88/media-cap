Introduction
============

MediaCap is a native media capture library for Windows, macOS and Linux. The C
API uses an opaque session handle. The C++17 API wraps that handle in a movable
session object and returns discovery results as C++ values.

- Discover monitors, windows, cameras, audio devices and audio processes.
- Record one video source and one or more audio sources. MediaCap resamples and
  mixes audio sources into one track.
- Record until you stop the session, or set a duration when starting. Paused
  intervals are omitted from the output timeline.
- Capture a monitor or window screenshot with an optional crop and output size.
- Receive borrowed video frames and audio blocks during recording or in a
  capture-only session.
- Inspect structured errors for startup, capture and output failures.

Source availability depends on the operating system, active desktop session,
device access and audio routing support. Query discovery and audio capabilities
before asking the user to select a source.

Record a monitor in C++
-----------------------

This program records the first discovered monitor for three seconds. Its output
is an H.264 video in an MP4 container. Run it in a desktop session that allows
screen capture.

.. code-block:: cpp

   #include <MediaCap.hpp>
   #include <chrono>
   #include <iostream>
   #include <thread>

   int main() {
       using namespace std::chrono_literals;

       const auto monitors = MediaCap::get_monitors();
       if (monitors.empty()) {
           std::cerr << "No monitor is available.\n";
           return 1;
       }

       MediaCap::Session session;
       session.add_source(monitors.front(), MediaCap::ScreenCaptureConfig{33});

       MediaCap::VideoEncoderConfig video{1280, 720, 30, 2'000'000,
                                          MEDIACAP_VIDEO_CODEC_H264};
       const MediaCap::RecordingConfig output{
           "monitor.mp4", MEDIACAP_CONTAINER_MP4, &video, nullptr};

       if (!session.start_recording(output, 3s)) {
           std::cerr << session.get_error().message << '\n';
           return 1;
       }
       while (session.is_recording()) {
           std::this_thread::sleep_for(20ms);
       }
       if (session.has_error()) {
           std::cerr << session.get_error().message << '\n';
           return 1;
       }
   }

The duration starts after each source delivers its first frame. Use
:doc:`examples/index` for device selection and additional capture tasks. See
:doc:`building` for compilation and :doc:`api/cpp` for the C++ API.
