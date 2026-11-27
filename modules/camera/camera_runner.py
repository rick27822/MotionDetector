"""Qt QProcess camera worker. See docs/qt-bridge.md for the wire format."""
import ipaddress
import io
import json
import os
from pathlib import Path
from queue import Empty, Queue
import struct
import sys
import time
from threading import Thread

from camera_module import CameraEngine, CaptureConfig, PreprocessConfig


def send(kind, payload=b"", **fields):
    header = json.dumps(dict(version=1, type=kind, payloadLength=len(payload), **fields),
                        ensure_ascii=False, allow_nan=False).encode("utf-8")
    stream = sys.stdout.buffer
    stream.write(struct.pack(">I", len(header)))
    stream.write(header)
    stream.write(payload)
    stream.flush()


def commands(queue):
    with os.fdopen(os.dup(sys.stdin.fileno()), "rb") as stream:
        for line in stream:
            try:
                command = json.loads(line.decode("utf-8"))
                if not isinstance(command, dict):
                    raise ValueError("命令应为 JSON 对象")
            except (UnicodeError, ValueError) as exc:
                command = {"command": "invalid", "detail": str(exc)}
            queue.put(command)
    queue.put(None)


def main():
    from camera_module.phone import PhoneServer

    queue = Queue(maxsize=32)
    Thread(target=commands, args=(queue,), daemon=True).start()
    engine = CameraEngine()
    phone = None
    active = False
    last_id = -1
    last_status = None
    session = ""
    last_ms = -1

    def stop():
        nonlocal phone, active, session, last_id, last_ms, last_status
        if phone:
            phone.stop()
            phone = None
        engine.stop()
        active, session, last_id, last_ms, last_status = False, "", -1, -1, None

    try:
        send("ready")
        while True:
            try:
                command = queue.get(block=not active)
            except Empty:
                command = {}
            else:
                if command is None:
                    break
                request = command.get("requestId")
                name = command.get("command")
                try:
                    if name == "start":
                        source = command["source"]
                        capture = CaptureConfig(**command.get("capture", {}))
                        capture.validate()
                        if source == "phone":
                            host = str(ipaddress.ip_address(command["host"]))
                            candidate = PhoneServer(engine, Path(__file__).parent / "runtime",
                                                    host, command.get("port", 8443))
                        elif not (type(source) is int and source >= 0 or
                                  isinstance(source, str) and (source == "demo" or Path(source).is_file())):
                            raise ValueError("输入源应为设备编号、视频路径、demo 或 phone")
                        stop()
                        if source == "phone":
                            phone = candidate
                            phone.start()
                            import qrcode
                            qr = io.BytesIO()
                            qrcode.make(phone.url).save(qr, format="PNG")
                            send("pairing", qr.getvalue(), url=phone.url, caPath=str(phone.ca_path),
                                 requestId=request)
                        else:
                            engine.start(source, capture)
                        active = True
                    elif name == "configure":
                        engine.configure(PreprocessConfig(**command["processing"]))
                    elif name == "stop":
                        stop()
                    else:
                        raise ValueError(command.get("detail", "未知命令"))
                    send("ack", requestId=request, command=name)
                except (ValueError, KeyError, TypeError, OSError, RuntimeError) as exc:
                    send("error", requestId=request, message=str(exc))
                continue

            result = engine.read_frame(last_id, timeout=.05)
            if result.status == "ok":
                packet = result.packet
                if packet.session_id != session:
                    session, last_ms = packet.session_id, -1
                last_id = packet.frame_id
                stamp = round((packet.monotonic_at - packet.session_started_at) * 1000)
                if stamp <= last_ms:
                    continue
                last_ms, last_status = stamp, "running"
                width, height = packet.source_size
                if width * height * 3 > 32 * 1024 * 1024:
                    send("status", state="error", sessionId=session, message="原始帧超过 32 MiB")
                    stop()
                    continue
                send("frame", memoryview(packet.source_bgr).cast("B"), sessionId=session,
                     sourceFrameId=last_id, timestampMs=stamp, width=width, height=height,
                     stride=width * 3, pixelFormat="BGR8", sourceToOutput=packet.source_to_output.tolist(),
                     outputSize=[packet.image.shape[1], packet.image.shape[0]], configRevision=packet.config_revision)
            elif result.status not in ("timeout", "session_changed"):
                if result.status != last_status:
                    send("status", state=result.status, sessionId=session, message=result.message)
                    last_status = result.status
                if phone:
                    # The phone service accepts a new peer without restarting the worker.
                    time.sleep(.035)
                else:
                    active = False
    finally:
        stop()


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        pass
