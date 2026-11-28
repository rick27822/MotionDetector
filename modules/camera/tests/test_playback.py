from pathlib import Path
import tempfile
import time
import unittest
import json

import av
import numpy as np

from camera_module import CameraEngine, PreprocessConfig
from playback_runner import IndexedRecording


class PlaybackTests(unittest.TestCase):
    def make_clip(self, directory):
        with CameraEngine(PreprocessConfig(64, 64)) as engine:
            sid = engine.start_external()
            for n in range(16):
                packet = engine.ingest(np.full((48, 64, 3), n*12, np.uint8), sid)
                if n == 0:
                    engine.start_recording(directory)
                time.sleep(.004 + (n % 3) * .002)
            return Path(engine.stop_recording()["path"]), packet.frame_id

    def test_seek_decodes_indexed_pixels(self):
        with tempfile.TemporaryDirectory() as tmp:
            path, last = self.make_clip(tmp)
            with av.open(str(path)) as movie:
                expected = {f.pts: f.to_ndarray(format="bgr24") for f in movie.decode(video=0)}
            rows = [json.loads(s) for s in (path.parent/"frames.jsonl").read_text().splitlines()]
            recording = IndexedRecording(path)
            try:
                for ms in [recording.duration_ms, 0, recording.duration_ms//2, 0, recording.duration_ms]:
                    frame, meta = recording.at(ms)
                    row = next(r for r in reversed(rows) if r["pts"] <= ms*90)
                    self.assertEqual((meta["sourceFrameId"],meta["timestampMs"]), (row["sourceFrameId"],row["timestampMs"]))
                    np.testing.assert_array_equal(frame, expected[row["pts"]])
                self.assertEqual(meta["sourceFrameId"], last)
            finally:
                recording.container.close()

    def test_damaged_index_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path, _ = self.make_clip(tmp)
            index = path.parent/"frames.jsonl"
            rows = index.read_text().splitlines()
            index.write_text("\n".join([rows[0], rows[0], *rows[2:]]))
            with self.assertRaises(ValueError):
                IndexedRecording(path)

    def test_recorded_angles_follow_source_frame_after_seek(self):
        with tempfile.TemporaryDirectory() as tmp, CameraEngine(PreprocessConfig(32, 32)) as engine:
            sid = engine.start_external()
            image = np.zeros((32, 32, 3), np.uint8)
            engine.ingest(image, sid)
            rid = engine.start_recording(tmp)
            time.sleep(.015)
            target = engine.ingest(image, sid)
            ms = round((target.monotonic_at-target.session_started_at)*1000)
            angles = {"leftKnee": 82.5}
            self.assertFalse(engine.submit_recording_angles(rid, "other", target.frame_id, ms, angles))
            self.assertFalse(engine.submit_recording_angles("old", sid, target.frame_id, ms, angles))
            self.assertFalse(engine.submit_recording_angles(rid, sid, target.frame_id, ms, {"leftKnee": 999}))
            time.sleep(.015)
            engine.ingest(image, sid)
            self.assertTrue(engine.submit_recording_angles(rid, sid, target.frame_id, ms, angles))
            result = engine.stop_recording()
            self.assertEqual(result["framesWithAngles"], 1)
            self.assertFalse(engine.submit_recording_angles(rid, sid, target.frame_id, ms, angles))
            recording = IndexedRecording(result["path"])
            try:
                for position in (2, 1, 0, 1):
                    _, meta = recording.at((recording.pts[position]+89)//90)
                    self.assertEqual(meta["anglesDeg"], angles if position==1 else {})
            finally:
                recording.container.close()
