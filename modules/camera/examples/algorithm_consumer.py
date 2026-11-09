"""Algorithm integration: synthetic input by default; optional camera/video."""
import argparse
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from camera_module import CameraEngine, CaptureConfig, PreprocessConfig, save_frame


def main():
    parser = argparse.ArgumentParser(description="4.2 algorithm integration example")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--camera", type=int)
    source.add_argument("--video")
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--save-to", type=Path, help="Export the first frame here")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    source = args.camera if args.camera is not None else args.video or "demo"
    with CameraEngine(PreprocessConfig(256, 256, color="RGB")) as camera:
        camera.start(source, CaptureConfig())
        last_id, saved = -1, False
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            result = camera.read_frame(after_id=last_id, timeout=1)
            if result.status == "timeout":
                continue
            if result.status != "ok":
                print(result.status, result.message)
                return 1 if result.status == "error" else 0
            packet = result.packet
            last_id = packet.frame_id
            # points_in_output = your_pose_model(packet.image)
            # Convert normalized model coordinates to pixels FIRST, if applicable.
            # points_in_original = packet.to_source(points_in_output)
            # overlay = packet.source_bgr.copy()
            print(f"frame={packet.frame_id} session={packet.session_id[:8]} "
                  f"shape={packet.image.shape} color={packet.color} "
                  f"config={packet.config_revision} preprocess={packet.processing_ms:.2f}ms")
            if args.save_to and not saved:
                print("Saved:", save_frame(packet, args.save_to))
                saved = True
            time.sleep(.12)  # Slow consumer: skips older frames without blocking capture.
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
