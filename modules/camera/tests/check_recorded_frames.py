"""Decode a completed camera recording and check it against its source frame index."""
import json
from pathlib import Path
import sys

import av


def check(movie):
    summary = json.loads((movie.parent / "recording.json").read_text(encoding="utf-8"))
    with (movie.parent / "frames.jsonl").open(encoding="utf-8") as index, av.open(str(movie)) as video:
        count = 0
        previous = None
        for frame in video.decode(video=0):
            row = json.loads(next(index))
            if frame.pts != row["pts"] or (previous is not None and row["sourceFrameId"] != previous + 1):
                raise ValueError(f"Frame index mismatch at {count}")
            previous = row["sourceFrameId"]
            count += 1
        if next(index, None) is not None or not count or count != summary["frames"] or summary["endReason"] != "stopped":
            raise ValueError("Incomplete recording or frame count mismatch")
    print(f"{count} decoded frames, contiguous source IDs, matching PTS")


if __name__ == "__main__":
    check(Path(sys.argv[1]))
