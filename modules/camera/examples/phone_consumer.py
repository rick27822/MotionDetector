"""Headless phone receiver. Example: python examples/phone_consumer.py --host 192.168.1.2

Use a LAN address reachable by the phone; see README for certificate trust.
No camera is opened until the phone user explicitly starts browser capture.
"""
import argparse
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from camera_module import CameraEngine, PreprocessConfig
from camera_module.phone import PhoneServer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8443)
    parser.add_argument("--seconds", type=float, default=120)
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    runtime = Path(__file__).resolve().parents[1] / "runtime"
    with CameraEngine(PreprocessConfig(256, 256)) as engine:
        server = PhoneServer(engine, runtime, args.host, args.port)
        try:
            server.start()
            print("Pairing URL (keep private):", server.url)
            print("Public CA certificate:", server.ca_path)
            print("Waiting for phone; Ctrl+C stops the service.")
            last_id = -1
            deadline = time.monotonic() + args.seconds
            while time.monotonic() < deadline:
                result = engine.read_frame(after_id=last_id, timeout=1)
                if result.status == "ok":
                    packet = result.packet
                    last_id = packet.frame_id
                    print("frame", last_id, "session", packet.session_id[:8], packet.image.shape)
                elif result.status == "error":
                    raise RuntimeError(result.message)
                else:
                    time.sleep(.1)  # Avoid spinning while disconnected or not yet started.
        except KeyboardInterrupt:
            pass
        finally:
            server.stop()


if __name__ == "__main__":
    main()
