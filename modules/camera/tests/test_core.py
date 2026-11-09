import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import cv2
import numpy as np

from camera_module import CameraEngine, CaptureConfig, PreprocessConfig, preprocess
from camera_module.core import transform_points


class PreprocessingTests(unittest.TestCase):
    def test_rgb_gray_and_normalization(self):
        frame = np.zeros((16, 16, 3), np.uint8)
        frame[:] = (0, 0, 255)
        rgb, _, _ = preprocess(frame, PreprocessConfig(16,16))
        np.testing.assert_array_equal(rgb[4,4], [255,0,0])
        gray, _, _ = preprocess(frame, PreprocessConfig(16,16,color="GRAY"))
        self.assertEqual(gray.shape, (16,16))
        self.assertAlmostEqual(int(gray[4,4]), 76, delta=1)
        normalized, _, _ = preprocess(frame, PreprocessConfig(16,16,normalize=True))
        self.assertEqual(normalized.dtype, np.float32)
        np.testing.assert_array_equal(normalized[4,4], [1,0,0])
        self.assertFalse(normalized.flags.writeable)

    def test_letterbox_preserves_shape_and_maps_pixel_centers(self):
        frame = np.full((100,200,3), 250, np.uint8)
        output, _, matrix = preprocess(frame, PreprocessConfig(640,640))
        self.assertTrue(np.all(output[:160] == 24))
        self.assertTrue(np.all(output[160:480] == 250))
        self.assertTrue(np.all(output[480:] == 24))
        np.testing.assert_allclose(transform_points([[0,0],[199,99]], matrix), [[1.1,161.1],[637.9,477.9]])

    def test_rotation_mirror_matches_actual_marker_pixels(self):
        frame = np.zeros((24,32,3), np.uint8)
        points = np.array([[3,4],[21,13],[30,22]])
        for i,(x,y) in enumerate(points): frame[y,x] = (30+i*50, 100, 200)
        for angle in (0,90,180,270):
            for mirror in (False,True):
                with self.subTest(angle=angle,mirror=mirror):
                    w,h = (24,32) if angle in (90,270) else (32,24)
                    output, _, matrix = preprocess(frame, PreprocessConfig(w,h,angle,mirror,"BGR"))
                    mapped = transform_points(points, matrix)
                    for original, target in zip(points,mapped):
                        x,y = target.astype(int); ox,oy = original
                        np.testing.assert_array_equal(output[y,x], frame[oy,ox])
                    np.testing.assert_allclose(transform_points(mapped,np.linalg.inv(matrix)),points)

    def test_invalid_data_and_config(self):
        with self.assertRaises(ValueError): preprocess(np.zeros((16,16),np.uint8),PreprocessConfig())
        with self.assertRaises(ValueError): PreprocessConfig(rotation=45).validate()
        with self.assertRaises(ValueError): PreprocessConfig(width=0).validate()
        with self.assertRaises(ValueError): CaptureConfig(fps=0).validate()


class LifecycleTests(unittest.TestCase):
    def setUp(self): self.engine = CameraEngine(PreprocessConfig(64,64))
    def tearDown(self): self.engine.stop()

    def test_demo_stop_restart_discards_old_frames(self):
        self.engine.start("demo", CaptureConfig(160,90,30))
        first = self.engine.latest(timeout=2)
        self.assertIsNotNone(first)
        time.sleep(.2)  # Simulate a slow algorithm consumer.
        later = self.engine.latest(after_id=first.frame_id,timeout=1)
        self.assertGreater(later.frame_id, first.frame_id+1)
        self.assertLess(time.monotonic()-later.monotonic_at, .2)
        self.engine.stop()
        self.assertIsNone(self.engine.latest())
        self.engine.start("demo", CaptureConfig(160,90,30))
        new = self.engine.latest(after_id=later.frame_id,timeout=2)
        self.assertIsNotNone(new)
        self.assertNotEqual(first.session_id,new.session_id)

    def test_external_rejects_late_old_session(self):
        old = self.engine.start_external()
        frame = np.zeros((20,40,3),np.uint8)
        first = self.engine.ingest(frame,old,1.5)
        points = [[0,0],[39,19],[20,10]]
        np.testing.assert_allclose(first.to_source(first.to_output(points)), points,atol=1e-10)
        self.assertEqual(first.media_time,1.5)
        self.engine.finish_external(old)
        self.assertIsNone(self.engine.ingest(frame,old))
        new = self.engine.start_external()
        self.assertIsNone(self.engine.ingest(frame,old))
        self.assertIsNotNone(self.engine.ingest(frame,new))

    def test_camera_open_failure_releases_handle(self):
        class BrokenCamera:
            released = False
            def isOpened(self): return False
            def release(self): self.released = True
        camera = BrokenCamera()
        with patch("camera_module.core.cv2.VideoCapture", return_value=camera):
            self.engine.start(77)
            deadline = time.monotonic()+2
            while self.engine.stats()["status"] == "opening" and time.monotonic()<deadline: time.sleep(.01)
            self.assertEqual(self.engine.stats()["status"],"error")
            self.assertEqual(self.engine.read_frame().status, "error")
            self.engine.stop()
        self.assertTrue(camera.released)

    def test_actual_video_file_decodes_and_reaches_eof(self):
        with tempfile.TemporaryDirectory() as folder:
            path = str(Path(folder)/"test.avi")
            writer = cv2.VideoWriter(path,cv2.VideoWriter_fourcc(*"MJPG"),20,(96,64))
            self.assertTrue(writer.isOpened())
            for i in range(6): writer.write(np.full((64,96,3),i*30,np.uint8))
            writer.release()
            self.engine.start(path)
            packet = self.engine.latest(timeout=2)
            self.assertEqual(packet.source_size,(96,64))
            deadline = time.monotonic()+3
            while self.engine.stats()["status"] != "ended" and time.monotonic()<deadline: time.sleep(.03)
            self.assertEqual(self.engine.stats()["status"],"ended")
            self.assertGreaterEqual(self.engine.latest().frame_id,6)
            last = self.engine.read_frame()
            self.assertEqual(last.status, "ok")
            self.assertEqual(self.engine.read_frame(after_id=last.packet.frame_id).status, "end_of_stream")
            self.engine.stop()

    def test_live_config_changes_next_frame(self):
        self.engine.start("demo",CaptureConfig(160,90,30))
        first = self.engine.latest(timeout=2)
        self.engine.configure(PreprocessConfig(32,64,color="GRAY",normalize=True))
        deadline = time.monotonic()+2
        while time.monotonic()<deadline:
            packet = self.engine.latest(after_id=first.frame_id,timeout=.2)
            if packet is not None and packet.color == "GRAY": break
        self.assertEqual(packet.image.shape,(64,32))
        self.assertEqual(packet.image.dtype,np.float32)


if __name__ == "__main__": unittest.main()
