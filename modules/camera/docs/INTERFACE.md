# 4.2 摄像头与图像处理模块：接口说明

版本：1.1；检查日期：2026-11-05。

## 先看应该交哪个文件

- 团队使用 Python：交 `camera_interface.py` 和本说明。文件包含配置类型、帧结构、状态结果和 Protocol；可单独导入阅读，无须安装 OpenCV。实际运行必须使用完整项目的 `camera_module.CameraEngine`。
- 团队要求 C++ `.h`：交 `camera_module.h`。它是 C++11/OpenCV 的完整接口约定，**没有对应 C++ 实现，不可直接链接运行，也不是 Python 程序的头文件**。
- 目前可运行版本采用 Python。C++ 与 Python 的命名和时间单位不同，不能直接混用。团队最终技术栈仍须统一。

`camera_interface.py` 与完整项目中的 `camera_module/interface.py` 内容一致。集成时以包内版本为准；不要在两个地方分别改出不同定义。接口版本见 `API_VERSION` / `kCameraApiVersion`。

## 模块范围与覆盖情况

| 能力 | 当前交付 |
|---|---|
| 电脑摄像头启动、停止、重启、编号选择 | 已实现；模拟失败与释放已测，实体摄像头待验收 |
| 视频文件读取、按源帧率播放、文件结束 | 已实现并用实际 AVI 文件验证 |
| 无摄像头演示输入 | 已实现，明确标注合成画面 |
| 手机扫码、HTTPS、WebRTC、配对与断开 | 已实现；本机合成视频编解码验证通过 |
| 手机断线重连、弱网降质量 | 已实现浏览器逻辑，实际手机与弱网待验收 |
| 目标分辨率和帧率配置 | 已实现，设备实际值以返回画面/统计为准 |
| 旋转、镜像、等比例缩放、居中补边 | 已实现并验证，补边灰度值0..255可配置 |
| RGB/BGR/GRAY、uint8/float32 [0,1] | 已实现并验证 |
| 原始图像、模型输入、预览图输出 | 已实现，返回独立于采集器生命周期的数据 |
| 关键点正反向坐标转换 | 已实现并覆盖全部旋转/镜像组合 |
| 超时、停止、文件结束、断开、错误、会话变化 | `read_frame()` 分别报告 |
| 多消费者读取最新帧 | 各自保存帧编号，可跳帧，不形成无限队列 |
| 采集中更新处理配置 | 已实现，每帧携带 `config_revision` |
| 单帧导出 | 原图PNG、处理图PNG、原精度NPY、JSON元数据 |
| GUI配置保存、原图/处理图切换 | 已实现 |

一次只使用一个视频源。设计图中的自动发现所有手机、设备型号/电量列表、跨网络连接没有实现；浏览器接入需要用户先扫码。姿态模型、骨架绘制、角度计算、动作评分、完整录像回放及报告由相应模块实现。这里提供原图和坐标信息供它们接入；最新帧接口允许丢弃旧帧，不承诺无损连续录像。

## 公共调用对应表

| 功能 | Python可运行接口 | C++移植接口 |
|---|---|---|
| 处理参数更新/读取 | `configure(config)` / `processing_config()` | `Configure` / `GetProcessingConfig` |
| 启动电脑/视频/演示 | `start(source, capture)` | `Start(CameraConfig)` |
| 取帧并区分状态 | `read_frame(after_id, timeout, session_id)` | `ReadFrame(FrameRequest, out_frame)` |
| 简单取帧 | `latest(after_id, timeout)` | 三参数 `ReadFrame` 便捷重载 |
| 实测状态 | `stats()` | `GetStats()` |
| 手机连接层开始接入 | `start_external()`，返回令牌 | `BeginExternal(out_session_id)` |
| 手机提交已解码BGR帧 | `ingest(bgr, session, media_time)` | `PushFrame` |
| 手机会话结束 | `finish_external(session)` | `EndExternal` |
| 停止释放 | `stop()`；支持 `with` | `Stop()` |
| 坐标映射 | `packet.to_source/to_output` | 帧内3×3矩阵/逆矩阵 |
| 单帧导出 | 包导出的 `save_frame(packet, directory)` | 由存储层使用返回的Mat实现 |

Python 手机服务为 `camera_module.phone.PhoneServer(engine, runtime_dir, host, port=8443)`。`start()` 启动后从 `url` 获取扫码地址，`ca_path` 获取公开证书路径；`disconnect()` 返回 Future，可在控制线程 `result(timeout=8)` 等待；`stop()` 关闭服务。服务独立于图形界面，示例见完整项目 `examples/phone_consumer.py`。

## 处理、数据与时间约定

处理顺序：顺时针旋转 → 旋转后水平镜像 → 等比例缩放 → 居中补边 → 颜色转换 → 可选归一化。默认 RGB、640×640、uint8、补边值24。补边一侧多一个像素时，多余像素位于右侧/下侧。

- `CaptureConfig`：宽16..3840、高16..2160、帧率1..60，均须整数。设备不保证接受请求值。
- `PreprocessConfig`：宽/高16..1920，角度0/90/180/270，颜色RGB/BGR/GRAY；镜像/归一化必须为布尔值，补边灰度值是0..255整数。
- Python `image`：RGB/BGR为H×W×3，GRAY为H×W。归一化时float32，否则uint8。不是NCHW张量，不自带batch维度、均值/标准差标准化。
- `source_bgr`：处理前的uint8 BGR原图，用于原图叠加和录像；`preview_bgr`：旋转/镜像/缩放后的彩色BGR图。预览的颜色和数值类型不跟随模型输入的GRAY/归一化选项。
- 所有返回数组按只读约定共享；生产者不再改写。修改前用 `.copy()` / `.clone()`，不要重新打开数组可写标记。下一次采集、停止后，已取出的帧仍有效。
- `frame_id`：从1开始，在同一引擎对象生命周期内持续递增，重启不清零。`session_frames` 是当前会话计数；停止或外部断开后清零。
- `session_id`：每次开始生成。不要将上一会话的推理结果画到新会话画面上。若算法是异步的，还应对齐 `frame_id` 和 `config_revision`。
- `config_revision`：每次成功configure加1，构造时为0。已开始处理的帧可能在configure返回后以旧版本发布；如必须等待新设置，读取到目标版本的帧后再推理。
- Python时间/timeout均为秒；C++timeout为毫秒，接收时钟字段为微秒，`media_time_seconds`为秒。`processing_ms`两者均为毫秒。
- 本机接收Unix时间用于记录，单调时钟用于新鲜度。它们不是手机曝光时间，不能直接用来计算单向网络延迟。`media_time`未知时为None，C++使用有效标志。
- 坐标采用像素中心，左上角像素中心是(0,0)。矩阵包含缩放时的半像素偏移；模型输出0..1坐标时先按模型文档变成像素坐标再逆变换。超出原图的点不自动裁剪，由算法结合可见性过滤。

## 取帧和错误处理

推荐 `read_frame()`，返回 `FrameReadResult`；只有status为ok时packet非空。

| status | 调用方处理 |
|---|---|
| `ok` | 使用packet并更新自己的after_id |
| `timeout` | 当前没有更新帧，可继续等待 |
| `not_running` | 已停止或正在释放，结束读取或由控制层重新开始 |
| `end_of_stream` | 文件末帧已读完，结束循环 |
| `disconnected` | 手机已断开，等待连接层重连 |
| `error` | 读取message，提示用户修复设备/文件 |
| `session_changed` | 会话已切换，丢弃旧会话推理状态后重新读取 |

`after_id=None`或-1可取得当前最新帧；timeout=0不等待，负值、NaN/Inf等无效。非空session_id要求只接受指定会话；None接受当前会话。等待期间切换源会唤醒读取，不把新会话偷偷当作旧会话返回。`latest()`保留兼容语义，无新帧/结束/错误均返回None；需要区分时使用read_frame。

参数错误同步抛ValueError，且不停止原输入。`start()`通过检查后异步打开源，返回不代表设备已经成功打开；错误从read_frame/stats取得。OpenCV在文件读帧失败时没有统一的“损坏/正常结束”原因接口：首次无帧报错，读取过图像后停止按结束处理，损坏中段仍需结合文件检查。

控制操作由单一控制线程串行调用；读取、状态、configure可并发。外部帧由单一生产者提交，在ingest返回前不得修改输入。旧/空令牌返回None，旧断开通知不影响新会话。`stop()`唤醒读者，幂等；设备驱动3秒内无法退出会抛RuntimeError并保持stopping，暂时禁止新的采集，稍后重试stop。手机服务与本机输入切换时，先停止PhoneServer，再启动本机采集。

## 最小Python接入

把完整项目的 `camera_module/` 放入队友仓库中可导入的位置，安装项目依赖。不要只复制接口文件并期待它采集图像。

```python
from camera_module import CameraEngine, CaptureConfig, PreprocessConfig

with CameraEngine(PreprocessConfig(256, 256, color="RGB")) as camera:
    camera.start("demo", CaptureConfig())  # 改为0才打开实体摄像头
    last_id = -1
    for _ in range(60):
        result = camera.read_frame(after_id=last_id, timeout=1)
        if result.status == "timeout":
            continue
        if result.status != "ok":
            print(result.status, result.message)
            break
        packet = result.packet
        last_id = packet.frame_id
        model_input = packet.image
        original_for_overlay = packet.source_bgr.copy()
        # points = your_model(model_input)
        # original_points = packet.to_source(points)
```

完整示例：`examples/algorithm_consumer.py`，默认五秒结束；`--video 视频路径`可读视频，`--save-to 目录`可导出第一帧。

## 本分支文件位置

模块目录为 `modules/camera/`，C++ 头文件在 `docs/camera_module.h`，Python 接口在 `camera_module/interface.py`。从模块目录运行安装、演示和测试脚本。

## 集成前只需确认的项目约定

团队采用Python还是C++、模型输入尺寸/颜色/归一化、坐标约定，以及录像模块是否需要逐帧保存。若录像必须保留每一帧，需要另建有界队列或独立录制路径，不能依赖本接口的最新帧槽位。

## 技术依据

[Python Protocol](https://docs.python.org/3.12/library/typing.html#typing.Protocol)、[OpenCV VideoCapture](https://docs.opencv.org/4.x/d8/dfe/classcv_1_1VideoCapture.html)、[aiortc API](https://aiortc.readthedocs.io/en/latest/api.html)。实现按本机已安装依赖验证，版本见完整项目 requirements-tested.txt。
