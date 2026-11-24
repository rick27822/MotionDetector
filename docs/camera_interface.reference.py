"""摄像头与图像处理模块的 Python 接口草案，供团队讨论。

与已完成的 PoseLab-Camera/camera_module 包中 CameraEngine 的主要方法对应。
本文件是类型接口说明；实际运行需要该包、Python 3.10+、OpenCV 和 NumPy。
手机连接由单独的连接服务接入，不通过 start() 直接传手机地址。
"""
from typing import Protocol

from camera_module import CaptureConfig, FramePacket, PreprocessConfig


class CameraInterface(Protocol):
    def configure(self, config: PreprocessConfig) -> None:
        """设置输出尺寸、旋转、镜像、颜色与归一化；对后续帧生效。"""
        ...

    def start(self, source: int | str = "demo", capture: CaptureConfig | None = None) -> None:
        """启动输入：整数为电脑摄像头编号，字符串为视频文件路径或 demo。"""
        ...

    def latest(self, after_id: int | None = None, timeout: float = 0) -> FramePacket | None:
        """获取编号大于 after_id 的最新帧；timeout 单位为秒，0 表示不等待。

        无新帧时返回 None。处理较慢时允许跳过旧帧；遇到 None 时可通过
        stats() 查看是否出错或视频结束。每次新采集开始后重新设置 after_id。
        """
        ...

    def stats(self) -> dict:
        """返回状态、错误描述、帧率、会话编号等信息。"""
        ...

    def stop(self) -> None:
        """停止采集并释放资源；设备无法及时退出时可能抛出异常。"""
        ...


# FramePacket 的主要字段：
# image: NumPy 图像数组；按配置为 RGB、BGR 或灰度，默认 uint8。
# frame_id / session_id: 帧编号与采集会话编号。
# received_at: 电脑端进入图像处理时的 Unix 秒数。
# source_size: 原始图像尺寸 (宽, 高)。
# source_to_output: 原图像素坐标到输出图像素坐标的 3×3 矩阵。
# to_source(points): 将输出图坐标转换为原图坐标。
