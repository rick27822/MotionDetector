"""Serve indexed P4 frames from a completed 4.2 recording."""
from array import array
from bisect import bisect_right
from fractions import Fraction
import json
from pathlib import Path
import sys

import av

from camera_runner import send


class IndexedRecording:
    def __init__(self, path):
        path = Path(path)
        directory = path if path.is_dir() else path.parent
        meta = json.loads((directory / "recording.json").read_text(encoding="utf-8"))
        if (meta.get("schemaVersion", meta.get("version")) != 1 or meta.get("timeBase") != [1, 90000]
                or meta.get("video") != "video.mp4" or meta.get("frameIndex") != "frames.jsonl"
                or (not path.is_dir() and path.name not in ("video.mp4", "recording.json"))):
            raise ValueError("请选择 4.2 导出的 video.mp4 或 recording.json")
        self.session = meta["sessionId"]
        self.pts, self.ids, self.stamps = (array("q") for _ in range(3))
        self.index_path = directory / "frames.jsonl"
        self.angle_offsets = array("q")
        offset = 0
        with self.index_path.open("rb") as index:
            for line in index:
                row = json.loads(line)
                pts, identity, stamp = row["pts"], row["sourceFrameId"], row["timestampMs"]
                if (any(type(n) is not int or n < 0 for n in (pts, identity, stamp))
                        or (not self.pts and pts != 0)
                        or (self.pts and (pts <= self.pts[-1] or identity <= self.ids[-1]
                                         or stamp < self.stamps[-1]))):
                    raise ValueError("录像帧索引的顺序无效")
                self.pts.append(pts)
                self.ids.append(identity)
                self.stamps.append(stamp)
                self.angle_offsets.append(offset if row.get("anglesDeg") else -1)
                offset += len(line)
        if not self.pts or len(self.pts) != meta["frames"]:
            raise ValueError("录像与帧索引的数量不符")
        self.container = av.open(str(directory / "video.mp4"))
        try:
            self.stream = self.container.streams.video[0]
            if (self.stream.time_base != Fraction(1, 90000)
                    or self.stream.frames != len(self.pts) or self.stream.codec_context.name != "h264"):
                raise ValueError("视频轨道与录像索引不匹配")
            end = max(self.pts[-1]+1, self.stream.duration or 0)
            self.duration_ms = (end+89)//90
            self.decoder = self.container.decode(self.stream)
            self.position = -1
            self.image = None
        except Exception:
            self.container.close()
            raise

    def at(self, ms):
        pos = max(0, bisect_right(self.pts, ms*90)-1)
        if self.image is None or pos != self.position:
            if pos < self.position or pos > self.position + 6:
                self.container.seek(self.pts[pos], stream=self.stream, backward=True)
                self.decoder = self.container.decode(self.stream)
            for frame in self.decoder:
                if frame.pts == self.pts[pos]:
                    if frame.width * frame.height * 3 > 32 << 20:
                        raise ValueError("视频帧超过 32 MiB")
                    self.image = frame.to_ndarray(format="bgr24")
                    self.position = pos
                    break
                if frame.pts is None or frame.pts > self.pts[pos]:
                    raise ValueError("未找到索引对应的视频帧")
            else:
                raise ValueError("视频提前结束")
        next_ms = (self.pts[pos+1]+89)//90 if pos+1 < len(self.pts) else self.duration_ms
        angles = {}
        if self.angle_offsets[pos] >= 0:
            with self.index_path.open("rb") as index:
                index.seek(self.angle_offsets[pos])
                angles = json.loads(index.readline())["anglesDeg"]
        return self.image, dict(sourceFrameId=self.ids[pos], timestampMs=self.stamps[pos], anglesDeg=angles,
                                pts=self.pts[pos], nextMs=next_ms)


def main():
    recording = IndexedRecording(sys.argv[1])
    try:
        send("opened", durationMs=recording.duration_ms, sessionId=recording.session,
             frames=len(recording.pts))
        for line in sys.stdin.buffer:
            request = json.loads(line)
            ms = request["ms"]
            if type(ms) is not int or not 0 <= ms <= recording.duration_ms:
                raise ValueError("播放位置越界")
            image, info = recording.at(ms)
            height, width = image.shape[:2]
            send("frame", memoryview(image).cast("B"), width=width, height=height, ms=ms,
                 requestId=request["id"], **info)
    finally:
        recording.container.close()


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        pass
    except Exception as exc:
        send("error", message=str(exc))
