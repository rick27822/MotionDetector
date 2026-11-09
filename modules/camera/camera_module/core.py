from __future__ import annotations

import math
from collections import deque
from pathlib import Path
import threading
import time
import uuid

import cv2
import numpy as np


from .interface import (CaptureConfig, PreprocessConfig, FramePacket, FrameReadResult,
                        CameraStats, transform_points, validate_read)


def preprocess(bgr: np.ndarray, config: PreprocessConfig):
    """Rotate, mirror, letterbox and convert. Return image, BGR preview, affine map.

    Coordinates use pixel centers: integer (0, 0) is the first pixel center.
    The affine map includes OpenCV resize's half-pixel offset.
    """
    config.validate()
    if not isinstance(bgr, np.ndarray) or bgr.ndim != 3 or bgr.shape[2] != 3 or bgr.dtype != np.uint8:
        raise ValueError("输入必须为 H×W×3 的 uint8 BGR 图像")
    h, w = bgr.shape[:2]
    if h == 0 or w == 0:
        raise ValueError("输入图像不能为空")
    matrix = np.eye(3, dtype=np.float64)
    if config.rotation == 90:
        image = cv2.rotate(bgr, cv2.ROTATE_90_CLOCKWISE)
        matrix = np.array([[0, -1, h - 1], [1, 0, 0], [0, 0, 1]], dtype=float)
    elif config.rotation == 180:
        image = cv2.rotate(bgr, cv2.ROTATE_180)
        matrix = np.array([[-1, 0, w - 1], [0, -1, h - 1], [0, 0, 1]], dtype=float)
    elif config.rotation == 270:
        image = cv2.rotate(bgr, cv2.ROTATE_90_COUNTERCLOCKWISE)
        matrix = np.array([[0, 1, 0], [-1, 0, w - 1], [0, 0, 1]], dtype=float)
    else:
        image = bgr
    rh, rw = image.shape[:2]
    if config.mirror:
        image = cv2.flip(image, 1)
        matrix = np.array([[-1, 0, rw - 1], [0, 1, 0], [0, 0, 1]]) @ matrix
    scale = min(config.width / rw, config.height / rh)
    nw, nh = max(1, round(rw * scale)), max(1, round(rh * scale))
    left, top = (config.width - nw) // 2, (config.height - nh) // 2
    preview = np.full((config.height, config.width, 3), config.padding_value, dtype=np.uint8)
    preview[top:top+nh, left:left+nw] = cv2.resize(
        image, (nw, nh), interpolation=cv2.INTER_AREA if scale < 1 else cv2.INTER_LINEAR)
    sx, sy = nw / rw, nh / rh
    matrix = np.array([[sx, 0, left + (sx - 1)/2],
                       [0, sy, top + (sy - 1)/2], [0, 0, 1]]) @ matrix
    if config.color == "RGB":
        output = cv2.cvtColor(preview, cv2.COLOR_BGR2RGB)
    elif config.color == "GRAY":
        output = cv2.cvtColor(preview, cv2.COLOR_BGR2GRAY)
    else:
        output = preview.copy()
    if config.normalize:
        output = output.astype(np.float32) / 255.0
    for array in (output, preview, matrix):
        array.flags.writeable = False
    return output, preview, matrix


def demo_frame(index, width, height):
    """Clearly synthetic test chart. Never impersonates a camera or pose detector."""
    image = np.full((height, width, 3), (24, 20, 15), dtype=np.uint8)
    for x in range(0, width, 60):
        cv2.line(image, (x, 0), (x, height), (42, 37, 29), 1)
    for y in range(0, height, 60):
        cv2.line(image, (0, y), (width, y), (42, 37, 29), 1)
    cx = int(width * (0.5 + 0.23 * np.sin(index / 24)))
    cy = int(height * (0.52 + 0.16 * np.cos(index / 30)))
    cv2.circle(image, (cx, cy), max(12, min(width, height)//9), (224, 205, 0), 3)
    cv2.line(image, (cx-25, cy), (cx+25, cy), (0, 155, 255), 2)
    cv2.line(image, (cx, cy-25), (cx, cy+25), (0, 155, 255), 2)
    fs = max(.4, min(width / 1000, 1.2))
    cv2.putText(image, "SYNTHETIC DEMO / NO CAMERA", (20, 40), cv2.FONT_HERSHEY_SIMPLEX, fs, (210, 219, 226), 1, cv2.LINE_AA)
    cv2.putText(image, f"Frame {index:06d}   {width} x {height}", (20, height-25), cv2.FONT_HERSHEY_SIMPLEX, fs, (0, 155, 255), 1, cv2.LINE_AA)
    cv2.rectangle(image, (0, 0), (width-1, height-1), (0, 155, 255), 2)
    return image


class CameraEngine:
    """Single producer + latest frame slot; slow consumers do not block capture.

    Call start/stop from one control thread. Phone ingestion uses the session token
    to reject late frames from a previous connection. No model code runs here.
    """
    def __init__(self, processing: PreprocessConfig | None = None):
        self._processing = processing if processing is not None else PreprocessConfig()
        self._processing.validate()
        self._config_revision = 0
        self._session_frames = 0
        self._condition = threading.Condition(threading.RLock())
        self._stop = threading.Event()
        self._thread = None
        self._latest = None
        self._sequence = 0
        self._session = ""
        self._source = ""
        self._status = "idle"
        self._message = "请选择输入源并开始采集"
        self._times = deque(maxlen=90)

    def configure(self, config: PreprocessConfig) -> None:
        config.validate()
        with self._condition:
            self._processing = config
            self._config_revision += 1

    def processing_config(self) -> PreprocessConfig:
        with self._condition:
            return self._processing

    def stats(self) -> CameraStats:
        with self._condition:
            elapsed = self._times[-1] - self._times[0] if len(self._times) > 1 else 0
            age = time.monotonic() - self._times[-1] if self._times else None
            fps = (len(self._times)-1)/elapsed if elapsed > 0 and age < 2 and self._status == "running" else 0
            return {"status": self._status, "message": self._message,
                    "source": self._source, "fps": fps, "frame_age": age,
                    "frames": self._sequence, "session_id": self._session,
                    "session_frames": self._session_frames, "config_revision": self._config_revision}

    def latest(self, after_id: int | None = None, timeout: float = 0) -> FramePacket | None:
        return self.read_frame(after_id, timeout).packet

    def read_frame(self, after_id: int | None = None, timeout: float = 0,
                   session_id: str | None = None) -> FrameReadResult:
        validate_read(after_id, timeout)
        if session_id is not None and (not isinstance(session_id, str) or not session_id):
            raise ValueError("session_id 必须为非空字符串或 None")
        with self._condition:
            initial_session = self._session

            def ready():
                return self._latest is not None and (after_id is None or self._latest.frame_id > after_id)

            def terminal():
                return (self._status in ("idle", "stopping", "error", "ended")
                        or (self._status == "waiting" and not self._session)
                        or self._session != initial_session
                        or (session_id is not None and session_id != self._session))

            if not ready() and not terminal() and timeout:
                self._condition.wait_for(lambda: ready() or terminal(), timeout)
            if self._status in ("idle", "stopping"):
                return FrameReadResult("not_running", message=self._message)
            if self._status == "error":
                return FrameReadResult("error", message=self._message)
            if self._status == "waiting" and not self._session:
                return FrameReadResult("disconnected", message=self._message)
            if ((session_id is not None and session_id != self._session)
                    or self._session != initial_session):
                return FrameReadResult("session_changed", message="采集会话已改变，请重新获取会话编号")
            if ready():
                return FrameReadResult("ok", self._latest)
            if self._status == "ended":
                return FrameReadResult("end_of_stream", message=self._message)
            return FrameReadResult("timeout", message="等待新帧超时")

    def _new_session(self, source, status):
        with self._condition:
            self._session = uuid.uuid4().hex
            self._source, self._status = source, status
            self._latest = None
            self._times.clear()
            self._session_frames = 0
            self._message = "正在打开输入源" if status == "opening" else "等待手机视频"
            self._condition.notify_all()
            return self._session

    def start(self, source: int | str = "demo", capture: CaptureConfig | None = None) -> None:
        capture = capture if capture is not None else CaptureConfig()
        capture.validate()
        if not ((type(source) is int and source >= 0) or (isinstance(source, str) and source)):
            raise ValueError("输入源必须为非负摄像头编号、视频文件路径或 demo")
        if isinstance(source, str) and source != "demo" and not Path(source).is_file():
            raise ValueError("视频文件不存在")
        self.stop()
        self._stop.clear()
        session = self._new_session(str(source), "opening")
        self._thread = threading.Thread(target=self._capture_loop, args=(source, capture, session), daemon=True)
        self._thread.start()

    def start_external(self) -> str:
        self.stop()
        self._stop.clear()
        return self._new_session("phone", "waiting")

    def finish_external(self, session: str, message: str = "手机已断开，可重新连接") -> None:
        with self._condition:
            if session and session == self._session and self._source == "phone":
                self._session = ""  # Invalidate any in-flight preprocessing.
                self._status, self._message = "waiting", message
                self._latest = None
                self._times.clear()
                self._session_frames = 0
                self._condition.notify_all()

    def ingest(self, bgr: np.ndarray, session: str, media_time: float | None = None) -> FramePacket | None:
        received_at, monotonic_at = time.time(), time.monotonic()
        started = time.perf_counter()
        with self._condition:
            if not session or session != self._session or self._stop.is_set():
                return None
            config, source = self._processing, self._source
            revision = self._config_revision
        if media_time is not None and (isinstance(media_time, bool) or not isinstance(media_time, (int, float))
                                       or not math.isfinite(media_time) or media_time < 0):
            raise ValueError("media_time 必须为非负有限秒数或 None")
        output, preview, matrix = preprocess(bgr, config)
        original = bgr.copy()
        original.flags.writeable = False
        processing_ms = (time.perf_counter() - started) * 1000
        with self._condition:
            if not session or session != self._session or self._stop.is_set():
                return None
            self._sequence += 1
            self._session_frames += 1
            packet = FramePacket(self._sequence, session, source, received_at, monotonic_at,
                                 media_time, (bgr.shape[1], bgr.shape[0]), output, preview,
                                 matrix, config.color, config.normalize, processing_ms, original, revision)
            self._latest = packet
            self._times.append(monotonic_at)
            self._status, self._message = "running", "正在采集"
            self._condition.notify_all()
            return packet

    def _capture_loop(self, source, capture, session):
        cap = None
        try:
            if source != "demo":
                cap = cv2.VideoCapture(source)
                if not cap.isOpened():
                    raise RuntimeError("无法打开摄像头或视频。请检查设备编号、权限及是否被其他程序占用。")
                if isinstance(source, int):
                    cap.set(cv2.CAP_PROP_FRAME_WIDTH, capture.width)
                    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, capture.height)
                    cap.set(cv2.CAP_PROP_FPS, capture.fps)
                    cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
            rate = capture.fps
            if isinstance(source, str) and source != "demo":
                file_rate = cap.get(cv2.CAP_PROP_FPS)
                rate = file_rate if 1 <= file_rate <= 120 else capture.fps
            index, deadline = 0, time.monotonic()
            while not self._stop.is_set():
                if source == "demo":
                    bgr, media_time = demo_frame(index, capture.width, capture.height), index / rate
                else:
                    ok, bgr = cap.read()
                    if not ok:
                        if isinstance(source, str):
                            if index == 0:
                                raise RuntimeError("视频没有可解码的图像帧，请检查文件是否损坏或编码是否支持")
                            with self._condition:
                                if session == self._session and not self._stop.is_set():
                                    self._status, self._message = "ended", "视频播放完毕，可重新开始"
                                    self._condition.notify_all()
                            return
                        raise RuntimeError("摄像头读取失败，请检查连接后重新开始")
                    media_time = cap.get(cv2.CAP_PROP_POS_MSEC)/1000 if isinstance(source, str) else None
                self.ingest(bgr, session, media_time)
                index += 1
                deadline = max(deadline + 1 / rate, time.monotonic())
                self._stop.wait(max(0, deadline - time.monotonic()))
        except Exception as exc:
            with self._condition:
                if session == self._session and not self._stop.is_set():
                    self._status, self._message = "error", str(exc)
                    self._latest = None
                    self._condition.notify_all()
        finally:
            if cap is not None:
                cap.release()

    def stop(self) -> None:
        self._stop.set()
        with self._condition:
            self._session = ""
            self._latest = None
            self._times.clear()
            self._session_frames = 0
            self._status, self._message = "stopping", "已停止，摄像头资源正在释放"
            self._condition.notify_all()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=3)
            if self._thread.is_alive():
                raise RuntimeError("设备驱动仍在退出，请稍后重试；暂不启动新的采集")
        self._thread = None
        with self._condition:
            self._status, self._message = "idle", "已停止采集"
            self._condition.notify_all()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.stop()
