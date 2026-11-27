import json
from pathlib import Path
import tempfile
import unittest
from threading import Event
from unittest.mock import patch

import av
import numpy as np

from camera_module import CameraEngine, PreprocessConfig
from camera_module.recording import Recording


class RecordingTests(unittest.TestCase):
    def test_overload_finishes_a_contiguous_prefix(self):
        gate = Event()
        encode = Recording._encode
        def slow_start(recorder, draft):
            gate.wait(5)
            return encode(recorder, draft)
        with tempfile.TemporaryDirectory() as directory, CameraEngine(PreprocessConfig(32, 32)) as camera:
            sid = camera.start_external()
            img = np.zeros((32, 32, 3), np.uint8)
            first = camera.ingest(img, sid)
            with patch.object(Recording, "_encode", slow_start):
                camera.start_recording(directory)
                try:
                    for _ in range(40):
                        camera.ingest(img, sid)
                finally:
                    gate.set()
                info = camera.stop_recording()
            self.assertEqual(info["endReason"], "encoder_overload")
            rows = (Path(info["path"]).parent/"frames.jsonl").read_text().splitlines()
            self.assertEqual([json.loads(r)["sourceFrameId"] for r in rows],
                             list(range(first.frame_id, first.frame_id+info["frames"])))

    def test_mp4_pts_metadata_and_reconnect(self):
        with tempfile.TemporaryDirectory() as directory, CameraEngine(PreprocessConfig(64, 64)) as camera:
            source = camera.start_external()
            image = np.zeros((47, 65, 3), dtype=np.uint8)
            first = camera.ingest(image, source)
            identity = camera.start_recording(directory)
            expected = [first.frame_id]
            for shade in range(1, 10):
                image.fill(shade * 20)
                expected.append(camera.ingest(image, source).frame_id)
            camera.finish_external(source)
            other = camera.start_external()
            camera.ingest(image, other)
            result = camera.stop_recording()
            self.assertEqual((identity, source, "disconnected", len(expected)),
                             (result["recordingId"], result["sessionId"], result["endReason"], result["frames"]))
            movie = Path(result["path"])
            rows = [json.loads(line) for line in (movie.parent/"frames.jsonl").read_text().splitlines()]
            self.assertEqual(expected, [row["sourceFrameId"] for row in rows])
            with av.open(str(movie)) as container:
                stream = container.streams.video[0]
                self.assertEqual(stream.codec_context.name, "h264")
                decoded = list(container.decode(stream))
                self.assertEqual([f.pts for f in decoded], [r["pts"] for r in rows])
                self.assertEqual(stream.time_base.denominator, 90000)
                self.assertEqual((decoded[0].width, decoded[0].height), (66,48))
                self.assertGreater(decoded[-1].to_ndarray(format="bgr24").mean(), 155)
            camera.start_recording(directory)
            self.assertNotEqual(movie, Path(camera.stop_recording()["path"]))

    def test_capture_required_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory, CameraEngine() as camera:
            with self.assertRaises(RuntimeError):
                camera.start_recording(directory)
            sid = camera.start_external()
            camera.ingest(np.zeros((32,32,3), dtype=np.uint8), sid)
            original = Path(directory)/"existing.txt"
            original.write_text("keep")
            with self.assertRaises(OSError):
                camera.start_recording(original)
            camera.start_recording(directory)
            with self.assertRaises(RuntimeError):
                camera.start_recording(directory)
            camera.stop()
            self.assertEqual(camera.stop_recording()["endReason"], "capture_stopped")
            self.assertEqual(original.read_text(), "keep")
