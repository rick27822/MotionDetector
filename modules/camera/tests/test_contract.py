"""Regression tests for public API, lifecycle races and exported data."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

import cv2
import numpy as np

from camera_module import (CameraEngine, CameraInterface, CaptureConfig,
                           PreprocessConfig, preprocess, save_frame)


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.engine = CameraEngine(PreprocessConfig(32, 32))
        self.frame = np.full((20, 40, 3), (10, 80, 200), np.uint8)

    def tearDown(self):
        self.engine.stop()

    def test_complete_protocol_and_standalone_import(self):
        self.assertIsInstance(self.engine, CameraInterface)
        path = Path(__file__).resolve().parents[1] / "camera_module" / "interface.py"
        # -S excludes site-packages: the interface must not need OpenCV/NumPy at import.
        result = subprocess.run([sys.executable, "-S", "-c",
            "import runpy,sys; c=runpy.run_path(sys.argv[1]); c['CaptureConfig']().validate(); "
            "assert 'cv2' not in sys.modules; assert 'numpy' not in sys.modules", str(path)],
            capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_reject_bad_config_without_interrupting_session(self):
        session = self.engine.start_external()
        for cfg in [PreprocessConfig(width=16.5), PreprocessConfig(rotation=False),
                    PreprocessConfig(mirror="false"), PreprocessConfig(normalize=1),
                    PreprocessConfig(padding_value=256), CaptureConfig(fps=float("nan")),
                    CaptureConfig(width="640"), CaptureConfig(fps=True)]:
            with self.subTest(config=cfg), self.assertRaises(ValueError):
                cfg.validate()
        for source in [-1, True, None, 0.5, "", "this-file-does-not-exist.avi"]:
            with self.subTest(source=source), self.assertRaises(ValueError):
                self.engine.start(source)
            self.assertEqual(self.engine.stats()["session_id"], session)
        with self.assertRaises(ValueError):
            self.engine.configure(PreprocessConfig(width=1))
        self.assertEqual(self.engine.processing_config().width, 32)
        with self.assertRaises(ValueError):
            CameraEngine(PreprocessConfig(rotation=1))

    def test_read_validation_and_result_states(self):
        for timeout in [-1, float("nan"), float("inf"), True, "1"]:
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                self.engine.latest(timeout=timeout)
        for after in [-2, .5, True, "1"]:
            with self.subTest(after=after), self.assertRaises(ValueError):
                self.engine.read_frame(after_id=after)
        self.assertEqual(self.engine.read_frame().status, "not_running")
        session = self.engine.start_external()
        self.assertEqual(self.engine.read_frame(timeout=.02).status, "timeout")
        self.assertEqual(self.engine.read_frame(session_id="wrong").status, "session_changed")
        packet = self.engine.ingest(self.frame, session)
        result = self.engine.read_frame(session_id=session)
        self.assertEqual(result.status, "ok")
        self.assertIs(result.packet, packet)
        self.assertEqual(self.engine.read_frame(after_id=packet.frame_id).status, "timeout")
        self.engine.finish_external(session)
        self.assertEqual(self.engine.read_frame(session_id=session).status, "disconnected")
        self.assertIsNone(self.engine.ingest(self.frame, ""))
        self.assertIsNone(self.engine.ingest(self.frame, session))
        self.assertEqual(self.engine.stats()["session_frames"], 0)

    def test_stop_and_disconnect_wake_blocked_reader(self):
        for disconnect in (False, True):
            session = self.engine.start_external()
            with ThreadPoolExecutor() as pool:
                future = pool.submit(self.engine.read_frame, None, 5, session)
                time.sleep(.04)
                if disconnect:
                    self.engine.finish_external(session)
                else:
                    self.engine.stop()
                self.assertEqual(future.result(timeout=1).status,
                                 "disconnected" if disconnect else "not_running")

    def test_source_copy_padding_metadata_and_coordinate_mapping(self):
        self.engine.configure(PreprocessConfig(64, 64, rotation=90, mirror=True,
                                              normalize=True, padding_value=114))
        session = self.engine.start_external()
        packet = self.engine.ingest(self.frame, session, 1.25)
        self.frame[:] = 0
        np.testing.assert_array_equal(packet.source_bgr[0, 0], [10, 80, 200])
        np.testing.assert_allclose(packet.image[:, :16], 114 / 255, atol=1e-6)
        for array in [packet.image, packet.source_bgr, packet.preview_bgr, packet.source_to_output]:
            self.assertFalse(array.flags.writeable)
        pts = np.array([[0, 0], [39, 19], [20, 10]])
        np.testing.assert_allclose(packet.to_source(packet.to_output(pts)), pts, atol=1e-10)
        self.assertEqual(packet.to_source(np.empty((0, 2))).shape, (0, 2))
        with self.assertRaises(ValueError):
            packet.to_output([[float("nan"), 0]])
        json.dumps(packet.metadata(), allow_nan=False)
        self.assertEqual(packet.config_revision, 1)
        self.assertEqual(self.engine.stats()["session_frames"], 1)
        for media_time in [float("nan"), float("inf"), -1, True]:
            with self.assertRaises(ValueError):
                self.engine.ingest(self.frame, session, media_time)

    def test_inflight_config_and_stale_external_frame(self):
        entered, release = threading.Event(), threading.Event()
        def delayed(frame, config):
            entered.set()
            if not release.wait(2):
                raise TimeoutError("test preprocessing barrier")
            return preprocess(frame, config)
        old = self.engine.start_external()
        with ThreadPoolExecutor() as pool:
            with patch("camera_module.core.preprocess", side_effect=delayed):
                future = pool.submit(self.engine.ingest, self.frame, old)
                try:
                    self.assertTrue(entered.wait(1))
                    self.engine.configure(PreprocessConfig(64, 16, color="GRAY"))
                finally:
                    release.set()
                self.assertEqual(future.result(timeout=2).config_revision, 0)
            packet = self.engine.ingest(self.frame, old)
            self.assertEqual(packet.image.shape, (16, 64))
            self.assertEqual(packet.config_revision, 1)
            release.clear(); entered.clear()
            with patch("camera_module.core.preprocess", side_effect=delayed):
                future = pool.submit(self.engine.ingest, self.frame, old)
                try:
                    self.assertTrue(entered.wait(1))
                    new = self.engine.start_external()
                finally:
                    release.set()
                self.assertIsNone(future.result(timeout=2))
            self.engine.finish_external(old)
            self.assertEqual(self.engine.stats()["session_id"], new)
            self.assertIsNone(self.engine.latest())

    def test_stop_during_camera_read_cannot_overwrite_idle_state(self):
        reading, release, released = threading.Event(), threading.Event(), threading.Event()
        class FakeCamera:
            def isOpened(self): return True
            def set(self, *args): return True
            def read(self):
                reading.set(); release.wait(2)
                return False, None
            def release(self): released.set()
        with patch("camera_module.core.cv2.VideoCapture", return_value=FakeCamera()):
            self.engine.start(0)
            try:
                self.assertTrue(reading.wait(1))
                with ThreadPoolExecutor() as pool:
                    future = pool.submit(self.engine.stop)
                    time.sleep(.03)
                    self.assertEqual(self.engine.read_frame().status, "not_running")
                    release.set()
                    future.result(timeout=2)
            finally:
                release.set()
        self.assertTrue(released.is_set())
        self.assertEqual(self.engine.stats()["status"], "idle")

    def test_export_chinese_path_and_no_partial_snapshot_on_failure(self):
        session = self.engine.start_external()
        self.engine.configure(PreprocessConfig(32, 32, color="GRAY", normalize=True))
        packet = self.engine.ingest(self.frame, session)
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp) / "图像导出"
            saved = save_frame(packet, directory)
            np.testing.assert_array_equal(np.load(saved / "image.npy"), packet.image)
            decoded = cv2.imdecode(np.frombuffer((saved / "source.png").read_bytes(), np.uint8), cv2.IMREAD_COLOR)
            np.testing.assert_array_equal(decoded, packet.source_bgr)
            self.assertEqual(json.loads((saved / "metadata.json").read_text("utf-8"))["color"], "GRAY")
            second = save_frame(packet, directory)
            self.assertNotEqual(saved, second)
            with patch("camera_module.storage.cv2.imencode", return_value=(False, None)):
                with self.assertRaises(OSError):
                    save_frame(packet, directory)
            self.assertEqual(len(list(directory.iterdir())), 2)

    def test_empty_video_reports_error_instead_of_normal_eof(self):
        class EmptyVideo:
            def isOpened(self): return True
            def get(self, *args): return 30
            def read(self): return False, None
            def release(self): pass
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "empty.avi"
            path.touch()
            with patch("camera_module.core.cv2.VideoCapture", return_value=EmptyVideo()):
                self.engine.start(str(path))
                result = self.engine.read_frame(timeout=2)
                self.assertEqual(result.status, "error")
                self.assertIsNone(result.packet)


if __name__ == "__main__":
    unittest.main()
