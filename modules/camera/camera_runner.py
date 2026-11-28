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
from camera_module.core import is_rtsp_source


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
    recording_request = None
    pending_settings = None

    def apply_settings(settings):
        target, adaptive = settings
        if phone:
            phone.set_stream_settings(target, adaptive)
        else:
            engine.set_capture_target(target)

    def finish_recording(reason):
        nonlocal recording_request, pending_settings
        try:
            info = engine.stop_recording(reason)
            if info:
                send("recording", state="finished", requestId=recording_request, **info)
        except RuntimeError as exc:
            send("recording", state="failed", requestId=recording_request, message=str(exc))
        recording_request = None
        if pending_settings is not None:
            apply_settings(pending_settings)
            pending_settings = None

    def stop():
        nonlocal phone, active, session, last_id, last_ms, last_status
        finish_recording("capture_stopped")
        if phone:
            phone.stop()
            phone = None
        engine.stop()
        active, session, last_id, last_ms, last_status = False, "", -1, -1, None

    try:
        send("ready")
        while True:
            if engine.recording_finished():
                finish_recording("completed")
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
                            adaptive = command.get("adaptiveQuality", True)
                            if not isinstance(adaptive, bool):
                                raise ValueError("adaptiveQuality 应为布尔值")
                            host = str(ipaddress.ip_address(command["host"]))
                            candidate = PhoneServer(engine, Path(__file__).parent / "runtime",
                                                    host, command.get("port", 8443))
                        elif not (type(source) is int and source >= 0 or
                                  isinstance(source, str) and (source == "demo" or is_rtsp_source(source) or Path(source).is_file())):
                            raise ValueError("输入源应为设备编号、视频路径、demo 或 phone")
                        stop()
                        if source == "phone":
                            phone = candidate
                            phone.start()
                            import qrcode
                            qr = io.BytesIO()
                            pairing = (f"{phone.url}&width={capture.width}&height={capture.height}"
                                       f"&fps={capture.fps}&adaptive={int(adaptive)}")
                            qrcode.make(pairing).save(qr, format="PNG")
                            send("pairing", qr.getvalue(), url=pairing, caPath=str(phone.ca_path),
                                 requestId=request)
                        else:
                            engine.start(source, capture)
                        active = True
                    elif name == "setCapture":
                        target = CaptureConfig(**command["capture"])
                        target.validate()
                        adaptive = command.get("adaptiveQuality", True)
                        if not isinstance(adaptive, bool):
                            raise ValueError("adaptiveQuality 应为布尔值")
                        if recording_request is not None:
                            pending_settings = (target, adaptive)
                        else:
                            apply_settings((target, adaptive))
                    elif name == "configure":
                        engine.configure(PreprocessConfig(**command["processing"]))
                    elif name == "stop":
                        stop()
                    elif name == "startRecording":
                        try:
                            identity = engine.start_recording(command["directory"])
                        except (OSError, ValueError, KeyError, TypeError, RuntimeError) as exc:
                            send("recording", state="failed", message=str(exc), requestId=request)
                        else:
                            recording_request = request
                            send("recording", state="started", recordingId=identity, requestId=request)
                        continue
                    elif name == "angles":
                        engine.submit_recording_angles(command.get("recordingId"), command.get("sessionId"),
                                                       command.get("sourceFrameId"), command.get("timestampMs"),
                                                       command.get("anglesDeg"))
                        continue
                    elif name == "stopRecording":
                        finish_recording("stopped")
                        continue
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
                     stride=width * 3, pixelFormat="BGR8", captureFps=engine.stats()["fps"],
                     sourceToOutput=packet.source_to_output.tolist(),
                     outputSize=[packet.image.shape[1], packet.image.shape[0]], configRevision=packet.config_revision,
                     **({"pipelineSample":phone.pipeline_sample} if phone and os.getenv("POSELAB_PHONE_PROFILE") else {}),
                     **(phone.device_metrics(session) if phone else {}))
            elif result.status not in ("timeout", "session_changed"):
                if result.status != last_status:
                    message = "等待手机扫码连接" if phone and result.status == "not_running" else result.message
                    send("status", state=result.status, sessionId=session, message=message)
                    last_status = result.status
                if phone:
                    # The phone service accepts a new peer without restarting the worker.
                    time.sleep(.035)
                else:
                    finish_recording(result.status)
                    active = False
    finally:
        stop()


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        pass
