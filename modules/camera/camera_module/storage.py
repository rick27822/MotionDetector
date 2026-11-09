"""Single-frame export shared by the GUI and algorithm callers."""
import json
from pathlib import Path
import shutil
import tempfile
import uuid

import cv2
import numpy as np

from .interface import FramePacket


def _png(path, image):
    # imencode + pathlib supports Chinese paths on Windows.
    ok, data = cv2.imencode(".png", image)
    if not ok:
        raise OSError("PNG 编码失败")
    path.write_bytes(data.tobytes())


def save_frame(packet: FramePacket, directory: str | Path) -> Path:
    """Export output.png, source.png, image.npy and metadata.json in a new folder.

    A unique directory is published only after all files are written. No existing
    snapshot is overwritten. Raises OSError for filesystem failures.
    """
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=".frame-", dir=directory))
    destination = directory / f"frame_{packet.frame_id}_{uuid.uuid4().hex}"
    try:
        image = packet.image
        if packet.normalized:
            image = np.rint(image * 255).clip(0, 255).astype(np.uint8)
        if packet.color == "RGB":
            image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)
        _png(temporary / "output.png", image)
        _png(temporary / "source.png", packet.source_bgr)
        np.save(temporary / "image.npy", packet.image, allow_pickle=False)
        (temporary / "metadata.json").write_text(
            json.dumps(packet.metadata(), ensure_ascii=False, indent=2, allow_nan=False), encoding="utf-8")
        temporary.rename(destination)
        return destination
    except Exception:
        # This path was created exclusively by mkdtemp above.
        shutil.rmtree(temporary)
        raise
