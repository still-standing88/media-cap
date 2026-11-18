import concurrent.futures
import gc
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import weakref

import mediacap as mc


class SessionTests(unittest.TestCase):
    def test_feature_requirements(self):
        screen = mc.get_feature_requirements(mc.Feature.SCREEN)
        self.assertTrue(screen.included_in_build)
        self.assertTrue(screen.prerequisites & int(mc.Prerequisite.DISPLAY_SESSION))
        process = mc.get_feature_requirements(mc.Feature.PROCESS_AUDIO)
        self.assertEqual(process.platform, screen.platform)
        self.assertTrue(process.prerequisites & int(mc.Prerequisite.FLEXAUDIO))
        if process.platform == mc.Platform.WINDOWS and process.included_in_build:
            self.assertEqual(process.minimum_os_build, 20348)

    def tearDown(self):
        mc.set_error_callback(None)
        mc.clear_last_error()

    def test_lifecycle(self):
        session = mc.Session()
        with session as current:
            self.assertIs(current, session)
            self.assertGreater(session.id(), 0)
            self.assertFalse(session.is_capturing())
            self.assertFalse(session.is_recording())
            self.assertFalse(session.is_paused())
            self.assertFalse(session.has_error())
            self.assertEqual(session.get_error().code, mc.ErrorCode.NONE)
            session.stop_capture()
            session.stop_recording()
        session.close()
        with self.assertRaisesRegex(RuntimeError, "closed"):
            session.start_capture()

    def test_close_races_with_polling(self):
        session = mc.Session()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            def poll():
                for _ in range(200):
                    try:
                        self.assertFalse(session.is_capturing())
                    except RuntimeError as error:
                        self.assertIn("closed", str(error))
                        break

            tasks = [pool.submit(poll) for _ in range(3)]
            tasks += [pool.submit(session.close)]
            for task in tasks:
                task.result(5)

    def test_callback_cycle_is_collectable(self):
        session = mc.Session()
        self.assertTrue(session.set_frame_callbacks(lambda *args, session=session: None))
        reference = weakref.ref(session)
        del session
        gc.collect()
        self.assertIsNone(reference())

    def test_last_reference_can_be_released_in_callback(self):
        owner = [mc.Session()]
        reference = weakref.ref(owner[0])
        seen = []

        def receive(error):
            owner.clear()
            seen.append(error.code)

        mc.set_error_callback(receive)
        with mc.Session() as reporting_session:
            self.assertFalse(reporting_session.start_capture())
        self.assertEqual(seen, [mc.ErrorCode.INVALID_ARGUMENT])
        self.assertIsNone(reference())

    def test_global_callback_snapshot_and_retained_session_error(self):
        first, second = [], []
        with mc.Session() as session:
            mc.set_error_callback(first.append)
            self.assertFalse(session.start_capture())
            self.assertTrue(session.has_error())
            error = session.get_error()
            self.assertEqual(error.code, mc.ErrorCode.INVALID_ARGUMENT)
            self.assertEqual(error.session_id, session.id())
            self.assertEqual(first[-1].message, error.message)
            mc.set_error_callback(second.append)
            self.assertFalse(session.start_capture())
            self.assertTrue(second)
            self.assertEqual(len(first), 1)
            self.assertEqual(mc.get_last_error().code, error.code)
            mc.clear_last_error()
            self.assertEqual(mc.get_last_error().code, mc.ErrorCode.NONE)
            self.assertEqual(session.get_error().code, error.code)

    def test_config_conversion(self):
        config = mc.default_audio_source_config()
        self.assertEqual(config.kind, mc.AudioCaptureKind.MIC)
        self.assertEqual((config.gain, config.mix_mic_gain, config.mix_system_gain), (1.0, 1.0, 1.0))
        config.device_id = "dévice 🎙"
        self.assertEqual(config.device_id, "dévice 🎙")
        config.device_id = None
        config.exclude_pids = [1, 4294967295]
        self.assertEqual(config.exclude_pids, [1, 4294967295])
        with self.assertRaises(ValueError):
            config.device_id = "bad\0id"
        with self.assertRaises(TypeError):
            config.exclude_pids = [-1]
        with mc.Session() as session:
            recording = mc.RecordingConfig()
            recording.output_filename = "bad\0filename"
            with self.assertRaises(ValueError):
                session.start_recording(recording)
            with self.assertRaises(TypeError):
                session.set_frame_callbacks(42)

    def test_format_preflight_uses_utf8_and_reports_stream_mismatch(self):
        config = mc.RecordingConfig()
        config.video_config = mc.VideoEncoderConfig()
        config.output_filename = Path("écran-画面-🎬.mkv")
        self.assertTrue(mc.validate_recording_format(config))
        config.output_filename = Path("écran-画面-🎬.wav")
        self.assertFalse(mc.validate_recording_format(config))
        self.assertIn("Video sources", mc.get_last_error().message)
        config.output_filename = "bad\udcff.mkv"
        with self.assertRaises(UnicodeEncodeError):
            mc.validate_recording_format(config)


@unittest.skipUnless(os.environ.get("MEDIACAP_TEST_SCREEN") == "1", "requires an accessible display")
class ScreenTests(unittest.TestCase):
    def setUp(self):
        self.monitor = mc.get_monitors()[0]

    def test_screenshot_and_owned_callback_frame(self):
        config = mc.ScreenshotConfig()
        config.output_width = 64
        config.output_height = 32
        screenshot = mc.capture_screenshot(self.monitor, config)
        self.assertIsNotNone(screenshot, mc.get_last_error().message)
        self.assertEqual((screenshot.width, screenshot.height), (64, 32))
        screenshot_pixels = memoryview(screenshot.data[0])
        del screenshot
        self.assertTrue(screenshot_pixels.readonly)
        self.assertEqual(len(screenshot_pixels), 64 * 32 * 4)

        delivered = threading.Event()
        frames = []
        rejected = []
        with mc.Session() as session:
            def receive(index, source, frame):
                if not frames:
                    frames.append((index, source, memoryview(frame.data[0])))
                    try:
                        session.stop_capture()
                    except RuntimeError as error:
                        rejected.append(str(error))
                    delivered.set()

            capture = mc.ScreenCaptureConfig()
            capture.capture_rate_ms = 33
            session.add_source(self.monitor, capture)
            self.assertTrue(session.set_frame_callbacks(video_callback=receive))
            self.assertTrue(session.start_capture(), session.get_error().message)
            self.assertTrue(delivered.wait(5))
            session.stop_capture()
        self.assertEqual(frames[0][:2], (0, mc.SourceType.MONITOR))
        self.assertGreater(len(frames[0][2]), 0)
        self.assertIn("inside MediaCap callbacks", rejected[0])

    def test_timed_recording_with_unicode_path(self):
        with tempfile.TemporaryDirectory() as temp, mc.Session() as session:
            path = Path(temp) / "écran-画面.mkv"
            capture = mc.ScreenCaptureConfig()
            capture.capture_rate_ms = 33
            session.add_source(self.monitor, capture)
            video = mc.VideoEncoderConfig()
            video.width = 128
            video.height = 96
            video.framerate = 30
            video.bitrate_bps = 500_000
            recording = mc.RecordingConfig()
            recording.output_filename = path
            recording.container = mc.ContainerFormat.MKV
            recording.video_config = video
            self.assertTrue(session.start_recording(recording, duration_ms=500), session.get_error().message)
            deadline = time.monotonic() + 8
            while session.is_recording() and time.monotonic() < deadline:
                time.sleep(0.025)
            self.assertFalse(session.is_recording())
            session.close()
            self.assertGreater(path.stat().st_size, 0)

    def test_interpreter_exit_stops_active_capture(self):
        subprocess.run([sys.executable, "-X", "faulthandler", "-c", """
import mediacap as mc
session = mc.Session()
config = mc.ScreenCaptureConfig()
config.capture_rate_ms = 33
session.add_source(mc.get_monitors()[0], config)
session.set_frame_callbacks(video_callback=lambda *_: None)
if not session.start_capture():
    raise RuntimeError(session.get_error().message)
"""], check=True, timeout=15)


if __name__ == "__main__":
    unittest.main()
