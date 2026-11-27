# 接收 4.2 的真实相机帧

## 六页主程序中的 P1 联调

相机代码在 camera-4.2，P1 页面基于陈柯睿分支 feat/chenkerui-ui-stage1 的提交 823175a。随附的 [P1 接入改动](p1-ui.patch) 可应用到该版本的仓库根目录。先用 `git apply --check <补丁路径>` 检查，再用 `git apply <补丁路径>` 应用。相机模块仍需单独安装 Python 依赖。

用 Qt Creator 打开主程序 motiondetecter/CMakeLists.txt，选择 Qt 6 桌面 Kit。在运行环境中将 POSELAB_CAMERA_DIR 设为包含 camera_runner.py 的相机模块绝对路径。默认使用模块里的 .venv；如使用其他解释器，可设置 POSELAB_PYTHON 为该解释器的绝对路径。打开 P1，在底部画面来源选择「电脑摄像头」或「手机扫码」。手机模式需选择手机能访问的电脑地址。通过「停止采集」释放设备，点击「开始采集」重新开始。

2026-11-27 在 Windows、Qt 6.8 上对该版本运行界面测试，29 项通过。P1 两次开启电脑摄像头，分别绘制 30 帧和 31 帧 1280×720 画面；核对渲染像素、停止及重启后的会话变化。通过主程序回放团队的 12 次合成姿态样例，计数为 12、丢帧为 0。本轮实体手机仅在此前的独立 Qt 预览窗口验证。P1 相机画面和姿态文件回放各自验证；真人骨架与计数等待真实推理模块。

给姿态模块的图像入口是 ui::CameraStream::frameReady(image, sessionId, sourceFrameId, timestampMs)。保留源帧信息，以便将姿态对应回相同画面。实时姿态结果的消费入口及会话 token 约定见 UI 分支的 docs/P1实时提示联调交接.md。

后续在正式 P1 完成实体手机复测。手机经同一 Wi-Fi 连接两次，各接收 30 帧 720×1280 画面，断开事件和新会话均已核对。

## 从 P1 录制 MP4

选电脑或手机摄像头，等首帧出现后点「录制 MP4」，选择保存目录。点「结束录像」，待编码结束后可点「打开录像」。每次录制创建独立子目录。手机断线、切换输入或停止采集时，编码器完成当前片段。

| 交付文件 | P4 和数据模块的用法 |
| --- | --- |
| video.mp4 | H.264、YUV420P、原始图像方向。奇数尺寸在右边或底边补一个像素 |
| recording.json | recordingId、采集 sessionId、原图尺寸、总帧数、时间基准及结束原因 |
| frames.jsonl | 每行一帧，含 sourceFrameId、会话单调毫秒 timestampMs、视频 pts 及可用的来源媒体时间。播放器时间（秒）等于 pts / 90000 |

视频时间从第一张写入图像起算，保留实际帧间隔。P4 可按视频时间定位索引行，再用 sessionId 和 sourceFrameId 匹配姿态。角度数据由姿态服务提供。实时预览可跳过旧帧；录像从采集源独立接收。若编码跟不上，保存已编码的连续片段，endReason 为 encoder_overload；分辨率变化会结束当前片段。

进程命令为 startRecording（directory 为目录）和 stopRecording。返回类型 recording，state 为 started、finished 或 failed。finished 的 path 指向已发布的 MP4；失败附 message。Python 调用方可使用 CameraEngine.start_recording(directory) 和 stop_recording() 获取相同的结果数据。

## 独立预览窗口

2026-11-26 在 Windows 和 Qt 6.8 上完成独立接收端实测。电脑摄像头两次启停，各接收 30 帧 1280×720 的 BGR 图像；手机经局域网 WebRTC 两次接入，各接收 30 帧 720×1280 图像，观察到断开通知和新的会话编号。原模块的 21 项回归测试通过。该记录覆盖相机到 Qt 示例窗口；团队 P1、真实姿态模型和长时间运行仍需联合验证。

在 Windows 的相机模块目录先运行 setup.bat。Qt 用 QProcess 启动模块虚拟环境的 Python，唯一脚本参数是 camera_runner.py 的绝对路径。预览示例在 examples/qt_preview，使用 Qt 6 Widgets 和 C++17。在 Qt Creator 打开 qt_preview.pro，运行参数依次填写 Python 路径、脚本路径、输入源。路径有空格时加双引号。

| 输入源参数 | 操作 |
| --- | --- |
| 0 | 打开默认电脑摄像头 |
| 0 --check | 启停两次，每次接收并转换 30 帧，检查源帧号、时间与新会话；通过时退出码为 0 |
| demo --check | 用合成图案做同样的检查 |
| 视频文件的绝对路径 | 解码已有视频 |
| phone 电脑的局域网IPv4 | 窗口显示配对码。手机在同一 Wi-Fi，用系统浏览器打开并开始采集；断开后可以再次配对 |

手机首次使用的证书步骤见模块 README。配对二维码含本次会话的连接凭据。关闭窗口会关闭输入管道并释放设备。

## 进程协议 v1

stdin 每行是一条 UTF-8 JSON 命令。程序启动先发 ready。Qt 应持续读取 stdout，包括等待停止应答期间。关闭 stdin 会结束进程。stderr 用于诊断。

~~~json
{"command":"start","requestId":"a","source":0,"capture":{"width":1280,"height":720,"fps":30}}
{"command":"configure","requestId":"b","processing":{"width":640,"height":640,"mirror":true}}
{"command":"stop","requestId":"c"}
~~~

source 可为非负设备编号、视频路径、demo 或 phone。手机命令还需 host 字段，port 默认 8443。capture 用于本地设备或演示源。configure 替换整套预处理配置，未指定字段取默认值。原始像素始终按原方向发送。命令返回 ack 或 error，带回 requestId。start 的 ack 表示请求已受理，收到首个 frame 才有实际画面。

stdout 每条消息依次包含 4 字节大端无符号头长度、UTF-8 JSON 头、payloadLength 字节载荷。接收端最多接受 64 KiB 的头和 32 MiB 的载荷。一次读取可能只有半条消息，也可能有多条。所有头包含 version、type、payloadLength。frame 携带连续排列的 BGR8 原始像素；pairing 携带二维码 PNG，头含 url 和公开证书 caPath；ready、ack、status、error 的载荷为空。

| 帧头字段 | 含义 |
| --- | --- |
| sessionId / sourceFrameId | 采集会话及源帧号。慢消费者可跳帧。推理结果应透传这两个值，用于对齐画面 |
| timestampMs | 从该次采集会话建立至该帧进入本机图像处理的单调毫秒。换会话时重新计时；相同毫秒保留先发送的帧。P1 超时计时应锚定同一时间轴 |
| width / height / stride / pixelFormat | 原图宽、高、每行字节数和 BGR8。stride 等于宽乘 3。可构造 QImage::Format_BGR888；跨线程或保留数据时须持有独立缓冲 |
| sourceToOutput / outputSize / configRevision | 原图像素到 Python 处理图的 3×3 变换、处理图尺寸和配置版本。模型若直接使用原图，应使用自身的坐标变换 |

手动停止以 stop 的 ack 为准；视频结束发 end_of_stream，设备故障发 error 状态，手机离线发 disconnected。手机服务打开后首次连接前也可能收到 not_running。无新帧期间由 Qt 定时检查超时。重连以新帧的 sessionId 切换会话并清除旧骨架。电脑故障后可重新发送 start。示例只保留最新画面；正式 P1 可在帧解析后把同一个源帧同时提交给 VideoView 和姿态模块。
