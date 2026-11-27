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
