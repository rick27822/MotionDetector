"""4.2 模块公共接口 v1.1（Python 3.10+）。

此文件可单独阅读/导入，不要求 OpenCV；图像及坐标运算需要 NumPy。
CameraInterface 是类型约定，运行时请创建 camera_module.CameraEngine。
控制方法由一个线程串行调用；读取、状态查询可与采集并发。
"""
from __future__ import annotations

from dataclasses import dataclass
from concurrent.futures import Future
import math
from typing import TYPE_CHECKING, Literal, Protocol, TypedDict, runtime_checkable

if TYPE_CHECKING:
    import numpy as np

API_VERSION = "1.1"
CameraStatus = Literal["idle", "opening", "waiting", "running", "ended", "error", "stopping"]
ReadStatus = Literal["ok", "timeout", "not_running", "end_of_stream", "disconnected", "error", "session_changed"]


def _integer(value, name, low, high):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"{name}必须是 {low} 至 {high} 范围内的整数")


@dataclass(frozen=True)
class CaptureConfig:
    width: int = 1280
    height: int = 720
    fps: int = 30

    def validate(self) -> None:
        _integer(self.width, "采集宽度", 16, 3840)
        _integer(self.height, "采集高度", 16, 2160)
        _integer(self.fps, "采集帧率", 1, 60)


@dataclass(frozen=True)
class PreprocessConfig:
    width: int = 640
    height: int = 640
    rotation: int = 0
    mirror: bool = False
    color: str = "RGB"
    normalize: bool = False
    padding_value: int = 24

    def validate(self) -> None:
        _integer(self.width, "处理宽度", 16, 1920)
        _integer(self.height, "处理高度", 16, 1920)
        if type(self.rotation) is not int or self.rotation not in (0, 90, 180, 270):
            raise ValueError("旋转角度必须为整数 0、90、180 或 270（顺时针）")
        if self.color not in ("RGB", "BGR", "GRAY"):
            raise ValueError("颜色格式必须为 RGB、BGR 或 GRAY")
        if type(self.mirror) is not bool or type(self.normalize) is not bool:
            raise ValueError("mirror 和 normalize 必须为布尔值")
        _integer(self.padding_value, "补边灰度值", 0, 255)


def validate_read(after_id, timeout) -> None:
    if after_id is not None and (type(after_id) is not int or after_id < -1):
        raise ValueError("after_id 必须为 None 或大于等于 -1 的整数")
    if isinstance(timeout, bool) or not isinstance(timeout, (int, float)) or not math.isfinite(timeout) or timeout < 0:
        raise ValueError("timeout 必须为有限的非负秒数")


def transform_points(points, matrix):
    """变换 N×2 像素中心坐标；不裁剪边界，允许空数组。"""
    import numpy as np
    points = np.asarray(points, dtype=np.float64)
    matrix = np.asarray(matrix, dtype=np.float64)
    if points.ndim != 2 or points.shape[1] != 2 or not np.all(np.isfinite(points)):
        raise ValueError("关键点应为有限数值组成的 N×2 数组")
    if matrix.shape != (3, 3) or not np.all(np.isfinite(matrix)) or not np.allclose(matrix[2], [0, 0, 1]):
        raise ValueError("坐标矩阵必须为有限的 3×3 仿射矩阵")
    return (np.c_[points, np.ones(len(points))] @ matrix.T)[:, :2]


@dataclass(frozen=True)
class FramePacket:
    frame_id: int
    session_id: str
    source: str
    received_at: float  # 本机 Unix 秒，不是手机拍摄时间。
    monotonic_at: float  # 本机单调时钟秒数。
    media_time: float | None
    source_size: tuple[int, int]  # (宽, 高)
    image: np.ndarray  # RGB/BGR: H×W×3; GRAY: H×W; uint8 或 float32 [0,1]。
    preview_bgr: np.ndarray  # 处理后几何形状的 uint8 BGR 彩色预览。
    source_to_output: np.ndarray  # 3×3，含缩放半像素偏移。
    color: str
    normalized: bool
    processing_ms: float
    source_bgr: np.ndarray  # 处理前原图，独立只读副本；叠加和录像可使用。
    config_revision: int

    def to_source(self, points):
        import numpy as np
        return transform_points(points, np.linalg.inv(self.source_to_output))

    def to_output(self, points):
        return transform_points(points, self.source_to_output)

    def metadata(self) -> dict:
        return {
            "api_version": API_VERSION,
            "frame_id": self.frame_id, "session_id": self.session_id,
            "source": self.source, "received_at": self.received_at,
            "monotonic_at": self.monotonic_at, "media_time": self.media_time,
            "source_size": list(self.source_size),
            "output_size": [self.image.shape[1], self.image.shape[0]],
            "color": self.color, "normalized": self.normalized,
            "dtype": str(self.image.dtype), "processing_ms": self.processing_ms,
            "config_revision": self.config_revision,
            "source_to_output": self.source_to_output.tolist(),
        }


class CameraStats(TypedDict):
    status: CameraStatus
    message: str
    source: str
    fps: float
    frame_age: float | None
    frames: int  # 引擎生命周期内计数，不是当前会话计数。
    session_frames: int
    session_id: str
    config_revision: int


@dataclass(frozen=True)
class FrameReadResult:
    status: ReadStatus
    packet: FramePacket | None = None
    message: str = ""


@runtime_checkable
class CameraInterface(Protocol):
    def configure(self, config: PreprocessConfig) -> None:
        """校验后原子替换处理配置；已在处理的帧可带旧 config_revision 返回。"""
        ...

    def processing_config(self) -> PreprocessConfig: ...

    def start(self, source: int | str = "demo", capture: CaptureConfig | None = None) -> None:
        """非负整数为摄像头编号；字符串为文件路径或 demo。异步打开，失败见 stats。"""
        ...

    def latest(self, after_id: int | None = None, timeout: float = 0) -> FramePacket | None:
        """兼容取帧。timeout 单位秒；无新帧返回 None，错误请查 read_frame/stats。"""
        ...

    def read_frame(self, after_id: int | None = None, timeout: float = 0,
                   session_id: str | None = None) -> FrameReadResult:
        """推荐取帧：区分超时/结束/断开/错误/会话改变；停止会唤醒等待者。

        每位消费者独立保存 after_id；慢消费者允许跳帧。session_id=None 表示
        接受当前会话，非空字符串表示只接受该会话。数组只读，修改须 copy()。
        """
        ...

    def stats(self) -> CameraStats: ...

    def start_external(self) -> str:
        """供手机连接层调用：停止旧输入，返回新会话令牌。"""
        ...

    def ingest(self, bgr: np.ndarray, session: str, media_time: float | None = None) -> FramePacket | None:
        """单生产者提交 uint8 BGR 帧；旧/空令牌返回 None，非法图像抛 ValueError。"""
        ...

    def finish_external(self, session: str, message: str = "手机已断开，可重新连接") -> None:
        """令当前手机会话失效并唤醒读者；旧会话通知不影响新会话。"""
        ...

    def stop(self) -> None:
        """可重复停止；清空最新帧。驱动三秒内未退出则抛 RuntimeError，禁止重启。"""
        ...

    def __enter__(self) -> CameraInterface: ...
    def __exit__(self, exc_type, exc_value, traceback) -> None: ...


@runtime_checkable
class PhoneConnectionInterface(Protocol):
    """可选手机连接层，对应 camera_module.phone.PhoneServer。

    构造参数为 engine、runtime_dir、host、port=8443；此接口只列公开能力。
    创建对象不启动服务，start() 成功后 ca_path 指向公开 CA 证书。
    """
    @property
    def url(self) -> str: ...
    @property
    def alive(self) -> bool: ...
    @property
    def message(self) -> str: ...

    def start(self) -> None:
        """启动 HTTPS/WebRTC，每次重新启动更换配对令牌；失败抛 RuntimeError。"""
        ...

    def disconnect(self) -> Future | None:
        """断开当前手机，服务继续运行。非服务线程可等待 Future.result(timeout)。"""
        ...

    def stop(self) -> None:
        """停止服务并释放连接，可重复调用。"""
        ...
