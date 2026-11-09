# PoseLab · 4.2 摄像头与图像处理模块

这是一个可独立运行的 Python 桌面模块。负责采集图像、统一图像格式，并向姿态识别模块提供最新帧。支持电脑摄像头、本地视频、合成演示画面，以及通过局域网 WebRTC 连接的手机摄像头。

界面分为「实时采集」「手机连接」「图像处理」。设计依据是已提供的全页面设计稿，交付范围为摄像头与图像处理部分。姿态模型、关节角度计算、动作评分、录像管理和分析报告属于其他模块。

## 本次完善（v1.1）

接口完整说明见 `docs/INTERFACE.md`，公共类型见 `camera_module/interface.py`。
新增明确取帧结果 `read_frame()`、处理前原图 `source_bgr`、配置版本 `config_revision`、当前会话帧数、可配置补边值。完善参数校验、停止/断开唤醒、旧会话隔离和手机服务重启；单帧导出现在包含原图、处理图、NumPy数组和JSON。界面增加原图切换，并修正Windows缩放下的布局。

`docs/camera_module.h` 是 C++ 移植约定，没有 C++ 实现。当前实际运行及团队集成使用 Python。团队主程序使用 Qt 6.5.3。此目录可独立运行，与 Qt 的调用方式和模型输入规格仍需联调。

## 1. 先运行起来

推荐 Windows + Python 3.11 或 3.12。此版本在 Windows / Python 3.12 环境验证。

1. 首次在本仓库运行时，在 `modules/camera/` 下双击 `setup.bat`，联网安装本模块依赖。
2. 安装完成后双击 `start.bat`。
3. 在「实时采集」中选择「演示画面」，点击「开始采集」，先检查程序能否运行。此模式生成测试图案，不会打开摄像头。
4. 选择「电脑摄像头」，设备编号通常从 `0` 开始，点击「开始采集」。如存在多个摄像头，可尝试 `1`、`2`。
5. 点击「停止」释放摄像头；选择「视频文件」可读取已有录像。

命令行等效操作：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-tested.txt
.\.venv\Scripts\python.exe app.py
```

`python app.py --demo` 可以直接进入合成画面演示。程序初次打开不会自动启动真实摄像头或手机服务。界面显示的是实测处理帧率、实际输入尺寸和预处理耗时，目标采集帧率不保证设备一定能够达到。

## 2. 完成方案与工作划分

```text
电脑摄像头 / 视频文件 ── OpenCV 读取 ──┐
                                     ├─ 旋转 → 镜像 → 等比例缩放与补边
手机浏览器 ── WebRTC ── Python 解码 ──┘                  ↓
                                   颜色转换 → 可选归一化
                                                        ↓
                                   最新帧槽位（容量一）
                                      ↓             ↓
                                   桌面预览       姿态识别模块
```

实现分为四层，队友可以单独复用底层：

| 层 | 文件 | 职责 |
|---|---|---|
| 采集与帧管理 | `camera_module/core.py` 中的 `CameraEngine` | 输入源生命周期、帧编号、会话标识、时间信息、最新帧读取 |
| 图像预处理 | 同文件中的 `preprocess` | 旋转、镜像、保持比例缩放、补边、颜色转换和归一化 |
| 手机接入 | `camera_module/phone.py`、`camera_module/web/` | 配对二维码、HTTPS、WebRTC 接收、断开、手机端有限重连和降质量 |
| 桌面界面 | `app.py` | 操作入口、画面预览、真实指标、配置保存、单帧导出 |

### 已覆盖的功能

- 电脑摄像头启动、停止和重新启动；输入失败显示错误。
- 本地视频按视频帧率读取，结束时显示播放完毕。
- 输入帧预处理及模型关键点坐标的正反向映射。
- 预处理参数保存后用于随后开始处理的帧；在途帧可能带旧config_revision返回。电脑采集参数下次开始时应用。
- 手机通过浏览器采集，电脑通过真实 WebRTC 接收并解码。
- 每次启动手机服务生成新的随机配对信息；同一时间接入一台手机。
- 手机端连接中断后最多尝试三次重连；无视频到达 15 秒后接收端释放连接。
- 手机端依据浏览器支持的 WebRTC 统计尝试降低质量；实际约束是否生效由设备决定。
- 保存一帧的原图PNG、处理图PNG、NumPy数组和JSON元数据；通过 `save_frame()` 也可在无界面模式导出。

### 当前实现边界

- 当前版本一次使用一个输入源，切换到电脑输入会停止手机服务。
- 不包含全局扫描并识别所有手机型号的设备列表，也不虚构手机电量、IP 设备名或网络延迟。
- `预处理耗时`只计图像处理时间，不代表手机到电脑的端到端延迟。
- 局域网连接不使用外部 STUN/TURN；跨网络连接、复杂校园网隔离需要额外部署。
- 单帧导出已实现；连续录像与角度数据存储由录像模块负责。
- 实际摄像头、手机浏览器、弱网效果需要在目标设备上验收。自动化验证使用合成图像和本机 WebRTC 连接。

## 3. 手机接入

### 启动服务

1. 手机与电脑连接同一 Wi-Fi。
2. 打开「手机连接」，选择电脑的局域网 IPv4 地址，例如 `192.168.x.x`。`127.0.0.1` 仅供本机测试，手机不能用这个地址连接电脑。
3. 点击「启动手机服务」。程序创建本地证书，并在所选 IP 的 `8443` 端口提供 HTTPS 页面。
4. 如果系统提示网络访问，由你按所在网络情况允许 Python 在专用网络通信。信令使用 TCP 8443，WebRTC 视频还需要动态 UDP；不要仅开放 TCP 就假定视频可达。

### 首次配置信任证书

浏览器调用摄像头需要安全上下文。二维码链接使用本地 HTTPS，首次需要把这个项目生成的公开 CA 证书安装到测试手机并信任。程序不会自动修改电脑或手机的证书信任设置。

1. 在电脑程序中点击「打开证书文件夹」。
2. 将 `runtime/certs/local-ca.cer` 通过 USB、隔空投送或自己的文件传输方式复制到手机。只复制 `.cer` 文件，`.key` 文件保留在电脑上。
3. iPhone/iPad：打开证书并安装描述文件，然后在「设置 → 通用 → 关于本机 → 证书信任设置」中为这个本地 CA 开启完全信任。具体菜单名称可能因系统版本而不同。
4. Android：在系统设置中搜索「安装证书」或「CA 证书」，导入此文件，按系统要求确认。不同品牌的菜单和浏览器策略可能不同。
5. 使用手机的系统浏览器打开二维码中的地址。确认 HTTPS 没有证书错误，再点击「开始采集」并允许摄像头。
6. 返回电脑「实时采集」查看画面。测试结束后可从手机设置移除这个测试 CA。

也可以用已有可信开发证书替换程序的证书生成逻辑。不要把“忽略证书错误”当作可靠的摄像头授权方案。

二维码携带本轮配对信息。重启手机服务后需要重新扫码。电脑 IP 改变时，也需要停止服务、选择新 IP、重新启动。

### 手机常见问题

| 现象 | 检查方式 |
|---|---|
| 页面打不开 | 确认同一 Wi-Fi、电脑 IP 正确、服务已启动；检查专用网络防火墙规则和 Wi-Fi 客户端隔离 |
| 有证书提示 | 确认手机安装的是当前项目生成的 CA，且已开启信任；检查手机与电脑时间 |
| 没有摄像头权限 | 使用系统浏览器，检查站点摄像头权限及 HTTPS 信任状态 |
| 本机能预览，电脑无画面 | 检查 Python 的 UDP 通信是否被阻止、网络是否隔离、电脑端错误状态 |
| 后台或锁屏后断开 | 保持采集页面在前台，重新点击开始；浏览器可能暂停后台摄像头 |
| 无法连接第二台手机 | 在电脑上断开当前手机后再连接；当前版本一次只接收一台 |

## 4. 提供给算法同学的接口

在同一 Python 项目里直接导入，不需要先启动桌面界面。

```python
from camera_module import CameraEngine, CaptureConfig, PreprocessConfig

with CameraEngine(PreprocessConfig(width=256, height=256, color="RGB")) as camera:
    camera.start(0, CaptureConfig(width=1280, height=720, fps=30))
    last_id = -1
    while True:
        packet = camera.latest(after_id=last_id, timeout=1)
        if packet is None:
            if camera.stats()["status"] in ("error", "ended", "idle"):
                break
            continue
        last_id = packet.frame_id
        model_input = packet.image
        # points_output = model(model_input)
        # points_original = packet.to_source(points_output)
```

完整演示：`python examples/algorithm_consumer.py`；可用 `--video 路径` 读取本地视频，`--save-to 目录` 导出第一帧。无界面手机示例为 `examples/phone_consumer.py`。它默认使用合成画面，五秒后结束；传入 `--camera 0` 才会打开真实设备。

| 字段/方法 | 含义 |
|---|---|
| `source_bgr` | 处理前原始uint8 BGR图像，只读；叠加绘制前先复制 |
| `config_revision` | 本帧使用的处理配置版本，configure成功后版本加1 |
| `image` | 模型输入。RGB/BGR 为 H×W×3，GRAY 为 H×W。默认 uint8、0–255；归一化后为 float32、0–1。只读数组，需要修改时先 `.copy()` |
| `frame_id` | 引擎生命周期内递增的处理帧编号；跳号表示消费者跳过了旧帧，不代表原始设备帧号 |
| `session_id` | 每次开始采集生成，避免把上一次采集结果用于新会话 |
| `received_at` | 本机进入图像处理时的 Unix 秒数；不是手机拍摄时间，也不能用于测量单向网络延迟 |
| `monotonic_at` | 本机单调时钟时间，可计算处理结果的新鲜度，不用于跨设备对时 |
| `media_time` | 文件或 WebRTC 视频自身时间；电脑摄像头为 `None`。不同来源不共用同一时钟起点 |
| `source_size` | 原始图像的 `(宽, 高)` |
| `source_to_output` | 原图像素中心坐标到输出像素中心坐标的 3×3 矩阵，包含旋转、镜像、缩放和补边 |
| `to_source(points)` | 将 N×2 的输出像素坐标还原到原图 |
| `to_output(points)` | 将 N×2 的原图像素坐标映射到模型输入 |
| `metadata()` | 可序列化为 JSON 的元数据 |
| `read_frame(after_id, timeout, session_id)` | 推荐接口，明确区分ok、timeout、not_running、end_of_stream、disconnected、error、session_changed；timeout单位秒 |
| `latest(after_id, timeout)` | 读取比指定编号更新的最新帧；慢消费者自动跳过旧帧 |

坐标采用像素中心约定：左上像素中心为 `(0, 0)`。如果模型返回 0–1 坐标，先按该模型的坐标约定转换为输出图像的像素坐标，再调用 `to_source`。补边区域的关键点反变换后可能在原图之外，应由算法模块依据可见性、置信度和有效区域过滤。

先与队友确定：输入尺寸、颜色格式、是否归一化、坐标约定、推理放在哪个进程。当前接口适用于同一 Python 进程；如果总项目采用其他语言或独立推理服务，需要增加跨进程适配层。

## 5. 测试与验收

双击 `test.bat` 或运行：

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

- `test_contract.py`：独立接口导入、参数边界、等待唤醒、配置并发、原图所有权、中文路径导出与失败清理。
- `test_core.py`：颜色与归一化、补边、全部旋转/镜像组合、坐标映射、慢消费、停止重启、旧会话拒绝、设备失败释放、真实视频文件解码、实时更新配置。
- `test_phone.py`：在本机启动 HTTPS 与 WebRTC 服务，客户端发送合成视频，经 ICE/DTLS/RTP 后在服务端解码；同时检查鉴权、连接冲突、断开重连、过期断开请求、畸形请求和同一服务对象重启。

真实设备验收时建议依次演示：本机摄像头 → 旋转与镜像 → 保存当前帧 → 停止重启 → 手机扫码 → 手机断开重连 → 给算法队友读取最新帧。实际摄像头和手机的实测结果记录在你们的项目验收表中。

## 6. 答辩时可以解释的关键点

1. 为什么等比例缩放并补边：避免拉伸改变人体比例，同时满足模型固定尺寸输入。
2. 为什么保留坐标变换：模型在缩放后的图像上输出关键点，显示时需要还原到原图。
3. 为什么只保存最新帧：实时检测关注最新动作；算法慢时跳过旧帧，避免延迟持续增加。
4. 为什么有两类时间：本机接收时间与视频自身时间用途不同，不能当成已同步的跨设备时间。
5. 为什么将 UI 与采集分开：队友可以不启动桌面界面，直接导入采集和处理模块。
6. 为什么手机需要 HTTPS：浏览器将摄像头访问限制在安全上下文中。

## 7. 技术参考

- [OpenCV VideoCapture](https://docs.opencv.org/4.x/d8/dfe/classcv_1_1VideoCapture.html)：摄像头和文件读取、设备释放。
- [aiortc API](https://aiortc.readthedocs.io/en/latest/api.html)：Python WebRTC 连接和媒体轨道。
- [MDN getUserMedia](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia)：浏览器摄像头访问、安全上下文与权限。

本项目测试依赖的精确版本见 `requirements-tested.txt`；`requirements.txt` 给出了允许的版本范围。首次启动界面会把当前 Python 自带的 Tcl/Tk 脚本复制到 `runtime/tcl/` 并使用相对路径加载，以适配此电脑的路径环境；不会修改系统 Python。完整验证结果见 `TEST_REPORT.md`。
