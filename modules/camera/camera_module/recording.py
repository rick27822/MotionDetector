"""Encode source frames independently of the latest-frame preview."""
from fractions import Fraction
import json
import math
import sqlite3
from pathlib import Path
from queue import Empty, Full, Queue
from tempfile import TemporaryDirectory
from threading import Event, Lock, Thread
from uuid import uuid4

import av
import cv2


class Recording:
    clock = Fraction(1, 90000)

    def __init__(self, directory, packet):
        self.directory = Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=True)
        av.codec.Codec("libx264", "w")
        self.session = packet.session_id
        self.size = packet.source_size
        self.id = uuid4().hex
        # Retain only the source image and scalar metadata for each queued frame.
        self.queue = Queue(maxsize=max(1, min(24, (64 << 20) // packet.source_bgr.nbytes)))
        self.closed = Event()
        self.done = Event()
        self.reason = ""
        self.error = ""
        self.result = None
        self.angle_lock = Lock()
        self.angles = sqlite3.connect("", check_same_thread=False)
        self.angles.execute("CREATE TABLE poses (frame INTEGER, ms INTEGER, data TEXT, PRIMARY KEY(frame, ms))")
        self.worker = Thread(target=self._run, name="mp4-encoder")
        self.worker.start()

    def end(self, reason):
        if not self.closed.is_set():
            self.reason = reason
            self.closed.set()

    def offer(self, packet):
        if self.closed.is_set():
            return
        if packet.session_id != self.session or packet.source_size != self.size:
            self.end("source_changed")
            return
        try:
            self.queue.put_nowait((packet.source_bgr, packet.frame_id, packet.monotonic_at,
                                   packet.session_started_at, packet.media_time))
        except Full:
            # Publish the continuous prefix, with the exact reason for early completion.
            self.end("encoder_overload")

    def submit_angles(self, recording_id, session, frame_id, ms, angles):
        if (recording_id != self.id or session != self.session or type(frame_id) is not int
                or type(ms) is not int or not 0 <= frame_id < 2**63 or not 0 <= ms < 2**63
                or not isinstance(angles, dict) or not 0 < len(angles) <= 12):
            return False
        channels = {"leftKnee","rightKnee","leftHip","rightHip","trunk","leftElbow",
                    "rightElbow","leftShoulder","rightShoulder","neck","leftAnkle","rightAnkle"}
        if any(k not in channels or type(v) not in (int, float) or not 0 <= v <= 180
               or not math.isfinite(v) for k, v in angles.items()):
            return False
        with self.angle_lock:
            if self.closed.is_set():
                return False
            self.angles.execute("INSERT OR REPLACE INTO poses VALUES (?, ?, ?)",
                                (frame_id, ms, json.dumps(angles, allow_nan=False)))
        return True

    def _join_angles(self, draft):
        count = 0
        index = draft / "frames.jsonl"
        merged = draft / "frames.new"
        with self.angle_lock, index.open(encoding="utf-8") as src, merged.open("w", encoding="utf-8") as out:
            for line in src:
                row = json.loads(line)
                match = self.angles.execute("SELECT data FROM poses WHERE frame=? AND ms=?",
                                            (row["sourceFrameId"], row["timestampMs"])).fetchone()
                row["anglesDeg"] = json.loads(match[0]) if match else {}
                count += int(match is not None)
                out.write(json.dumps(row, allow_nan=False) + "\n")
        merged.replace(index)
        return count

    def finish(self, reason):
        self.end(reason)
        self.worker.join()
        if self.error:
            raise RuntimeError(self.error)
        return self.result

    def _run(self):
        try:
            with TemporaryDirectory(prefix=".recording-", dir=self.directory) as temp:
                draft = Path(temp) / self.id
                draft.mkdir()
                summary = self._encode(draft)
                summary["framesWithAngles"] = self._join_angles(draft)
                (draft / "recording.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
                destination = self.directory / self.id
                draft.rename(destination)
                self.result = dict(summary, path=str(destination / "video.mp4"))
        except Exception as exc:
            self.error = str(exc)
        finally:
            self.closed.set()
            with self.angle_lock:
                self.angles.close()
            self.done.set()

    def _encode(self, draft):
        width, height = self.size
        origin = None
        count, pts = 0, -1
        with av.open(str(draft / "video.mp4"), "w", format="mp4",
                     options={"video_track_timescale": "90000"}) as container, \
                (draft / "frames.jsonl").open("w", encoding="utf-8") as metadata:
            stream = container.add_stream("libx264", rate=30)
            stream.width, stream.height = width + width % 2, height + height % 2
            stream.pix_fmt = "yuv420p"
            stream.time_base = stream.codec_context.time_base = self.clock
            stream.options = {"preset": "ultrafast", "tune": "zerolatency", "bf": "0"}
            while not self.closed.is_set() or not self.queue.empty():
                try:
                    image, frame_id, stamp, start, media_time = self.queue.get(timeout=.05)
                except Empty:
                    continue
                if origin is None:
                    origin = stamp
                pts = max(pts + 1, round((stamp - origin) * 90000))
                if width % 2 or height % 2:
                    image = cv2.copyMakeBorder(image, 0, height % 2, 0, width % 2, cv2.BORDER_REPLICATE)
                frame = av.VideoFrame.from_ndarray(image, format="bgr24")
                frame.pts, frame.time_base = pts, self.clock
                for compressed in stream.encode(frame):
                    container.mux(compressed)
                row = dict(sourceFrameId=frame_id, timestampMs=round((stamp-start)*1000),
                           pts=pts, sourceMediaSeconds=media_time)
                metadata.write(json.dumps(row, allow_nan=False) + "\n")
                count += 1
            if count == 0:
                raise RuntimeError("本次录制没有收到图像帧")
            for compressed in stream.encode():
                container.mux(compressed)
        return dict(schemaVersion=1, recordingId=self.id, sessionId=self.session, frames=count,
                    timeBase=[1, 90000], sourceSize=list(self.size), codec="h264",
                    video="video.mp4", frameIndex="frames.jsonl", endReason=self.reason)
