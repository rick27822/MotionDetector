# 陈柯睿 · Qt 界面与交互模块方案设计

> 本文为基于原始分工的初版。2026-11-22 核查队友实际交付并确认 Mac 验收、2027-01-03 截止后，实施以 [完整实施计划](陈柯睿_完整实施计划_截至2027-01-03.md) 为准；类型、Python 相机桥接、真实后端与回放等决策已更新。

编写日期：2026-11-22。

依据：项目根目录《动作捕捉检测系统_分工与计划.md》、六页《动作捕捉检测系统 - 全页面设计稿.pdf》，以及当前 `motiondetecter/` 的 Qt 工程。已逐页查看设计稿；当前代码只有 Qt 6.5 Widgets 空窗口。本文件是建议方案，所列目录、类与补充接口目前尚未实现。

## 1. 核心判断和任务边界

你负责系统全部六页的界面、交互和联调牵头工作。工作量不只是“还原六张图”，还包括视频显示、骨架对齐、配置同步、回放联动、导出反馈和异常状态。

采用 **Qt Widgets + 页面类 + 可复用组件 + 少量业务控制器 + 接口注入 + Mock 服务**。继续使用当前 CMake 工程，不切换 QML，不引入完整的复杂 MVC 框架。

职责划分：

| 部分 | 你的责任 | 外部能力负责人 |
| --- | --- | --- |
| 视频 | 显示帧、画面适配、播放控件、等待和错误提示 | 孙梁提供采集、录制、回放解码 |
| 姿态 | 在正确的视频区域调用绘制工具，显示角度与状态 | 张在庆提供关键点、角度、SkeletonPainter |
| 分析 | 显示指标、曲线、评分、历史对比，触发导出 | 陈艺夫提供算法、提示文案、存储与导出 |
| 配置 | 控件、编辑草稿、校验、提交、应用反馈 | 各模块负责应用参数；陈艺夫负责持久化 |

不要在页面里重复实现采集、关节角度计算、评分、数据库或 MP4 解码。

## 2. 推荐目录

现有 `motiondetecter/` 是 CMake 源码根目录。新增文件放在它下面；项目根目录保留分工、设计稿和文档。以下为目标结构，先按第 11 节逐步创建。

```text
motionDetecter/                         # 当前工作区
├── 动作捕捉检测系统_分工与计划.md
├── 动作捕捉检测系统 - 全页面设计稿.pdf
├── docs/
│   ├── 陈柯睿_UI模块方案设计.md         # 本文
│   ├── ui-theme.md                     # 建议新增：颜色、间距、状态、缩放规则
│   └── integration-checklist.md        # 建议新增：接口、单位、错误、验收约定
└── motiondetecter/                     # 当前 CMake 根目录
    ├── CMakeLists.txt
    ├── main.cpp                       # QApplication、选择 Mock/真实服务、装配依赖
    ├── mainwindow.h/.cpp/.ui           # 保留现有入口，变为应用外壳
    ├── app/
    │   ├── AppController.h/.cpp        # 路由、启动/退出、控制器装配
    │   ├── AppState.h/.cpp             # 当前设备/会话/配置与检测状态
    │   └── PageId.h                    # 六页枚举，避免散落数字 0~5
    ├── core/                          # 团队共享；变更共同确认
    │   ├── types.h                    # 已约定的公共结构
    │   └── interfaces/
    │       ├── ICameraSource.h
    │       ├── IPoseEstimator.h
    │       ├── IAnalysisService.h
    │       ├── IDataManager.h
    │       └── IPlaybackSource.h       # 建议补充，需与孙梁确认
    ├── modules/
    │   ├── camera/                    # 孙梁负责
    │   ├── pose/                      # 张在庆负责；含 SkeletonPainter
    │   ├── analysis/                  # 陈艺夫负责
    │   └── ui/                        # 你的主要工作区
    │       ├── CMakeLists.txt
    │       ├── shell/
    │       │   ├── Sidebar.h/.cpp      # 品牌、六个导航按钮、设备状态
    │       │   └── TopBar.h/.cpp       # 当前页标题、录制/推理状态
    │       ├── pages/
    │       │   ├── RealtimePage.h/.cpp/.ui
    │       │   ├── DevicePage.h/.cpp/.ui
    │       │   ├── DetectionConfigPage.h/.cpp/.ui
    │       │   ├── PlaybackPage.h/.cpp/.ui
    │       │   ├── AnalysisPage.h/.cpp/.ui
    │       │   └── SettingsPage.h/.cpp/.ui
    │       ├── widgets/
    │       │   ├── VideoView.h/.cpp
    │       │   ├── AngleHUD.h/.cpp
    │       │   ├── ChartView.h/.cpp
    │       │   ├── ScoreRing.h/.cpp
    │       │   ├── CardWidget.h/.cpp
    │       │   ├── DeviceCard.h/.cpp
    │       │   ├── SessionCard.h/.cpp
    │       │   ├── ModelCard.h/.cpp
    │       │   ├── SliderField.h/.cpp
    │       │   └── ToggleField.h/.cpp
    │       ├── controllers/
    │       │   ├── DetectionController.h/.cpp  # 实时管线、录制、姿态参数
    │       │   ├── DeviceController.h/.cpp     # 发现/配对/连接
    │       │   ├── SessionController.h/.cpp    # 回放、分析、导出
    │       │   └── SettingsController.h/.cpp   # 统一配置入口与持久化
    │       ├── adapters/
    │       │   └── FrameImageAdapter.h/.cpp   # BGR 帧到可安全显示的 QImage
    │       ├── theme/
    │       │   ├── Theme.h/.cpp         # 颜色、绘图参数、QSS 加载
    │       │   └── Style.qss
    │       └── resources/
    │           ├── ui.qrc
    │           ├── icons/*.svg
    │           └── images/empty-video.png
    └── mocks/
        ├── MockCameraSource.h/.cpp
        ├── MockPoseEstimator.h/.cpp
        ├── MockAnalysisService.h/.cpp
        ├── MockDataManager.h/.cpp
        ├── MockPlaybackSource.h/.cpp
        └── fixtures/                  # 固定样例、短视频与完整时间戳姿态序列
```

`Foo.h/.cpp` 表示两个文件。目录是边界规划，不要求第一天全部建好。CardWidget、四个控制器也可以在功能出现后再逐个提取。

建议 `.ui` 负责每页静态布局，C++ 负责动态卡片、信号连接和自绘组件。自绘组件在 Designer 中通过“提升为/Promote to”接入。不要手动修改自动生成的 `ui_*.h`。

## 3. 每层做什么、怎样连接

```text
main.cpp 创建真实服务或 Mock 服务
    ↓ 注入
AppController 装配 MainWindow 和四个控制器
    ↓
页面发出用户意图 → 控制器调用 I* 接口 → 服务完成/失败
    ↑                                      ↓
页面更新组件 ← 控制器更新共享状态或发出展示信号
```

### 主窗口和页面

- `MainWindow`：只创建外壳、六页与 `QStackedWidget`，暴露切页与访问页面的方法；不堆放所有业务。
- `Sidebar`：发出 `pageRequested(PageId)`；使用 `QButtonGroup` 排他选择。设备状态来自同一份共享状态。
- `TopBar`：根据当前页更新标题；P1 展示帧率、模型状态与录制时间，其他页可隐藏这些项。
- 页面：组装组件、读取控件草稿、发出操作信号、接收展示数据。

示意接口，非当前可编译实现：

```cpp
// RealtimePage
signals:
    void startDetectionRequested();
    void stopDetectionRequested();
    void startRecordingRequested();
    void stopRecordingRequested();
public slots:
    void showPose(const PoseResult &result);
    void setRecordingState(bool active);

// 配置页：点击保存才发出完整草稿
signals:
    void applyRequested(const PoseConfig &draft);
```

### 控制器

| 控制器 | 具体实现思路 |
| --- | --- |
| DetectionController | 控制取流与推理管线；将显示帧送入 VideoView，将匹配的 PoseResult 送入骨架/HUD；协调录制和姿态元数据提交；检测配置由 SettingsController 统一提交后执行 |
| DeviceController | 调用发现、连接、断开；处理异步状态、失败和超时；把设备信息同时更新到 P2、侧栏和共享状态 |
| SessionController | 选择会话、加载数据、驱动回放和曲线游标；在后台请求分析与导出；将结果送给 P4/P5；管理当前会话 ID |
| SettingsController | 读取配置并给 P3/P6 填值；维护校验与应用流程；向相机和姿态控制器分发参数；写入 IDataManager；同步 P1 参数摘要 |

AppState 只存少量跨页共享的当前状态，不保存所有视频帧或全部历史数据，也不负责调用 SDK。P3/P6 使用本地编辑草稿；保存成功后再更新共享状态。

同一进程内的服务由装配层统一创建、统一结束；页面不各自 new CameraManager。QObject 采用明确的 parent/析构所有权；移入工作线程的对象不能带 GUI 父对象；注入的非拥有指针必须确保服务比页面活得久。

## 4. 主框架与视觉规范

六页 PDF 均为 1440×900。按设计稿估计侧栏宽约 224、顶栏高约 48；这些是初始布局建议，不是已经测量冻结的规范。

```text
MainWindow.centralWidget
└── QHBoxLayout（margin=0, spacing=0）
    ├── Sidebar（宽约224）
    └── 右侧 QWidget / QVBoxLayout
        ├── TopBar（高约48）
        └── QStackedWidget（占剩余空间）
```

- 默认窗口 1440×900；使用 Qt 逻辑像素，让 Qt 处理高 DPI。
- 主要布局用 `QHBoxLayout/QVBoxLayout/QGridLayout`，卡片内部边距约 16~24，卡片间距约 16~24，再逐页对照设计稿微调。
- P1 视频区域贴近顶栏；P2~P6 内容通常有约 24 的页内边距，不能强迫六页使用同一外边距。
- 给窗口设经验证的最小尺寸；P3/P6 在较小窗口中使用 `QScrollArea`，不要将所有控件按固定坐标摆放。
- 建议主题初值：窗口黑色 `#000000`、侧栏/顶栏约 `#0B0B0B`、卡片约 `#101010`、次级容器约 `#161616`、主强调色 `#FF8A00`、骨架/角度青色 `#00D9EE`。这些颜色需截图比对后调整。
- `Style.qss` 负责原生控件背景、边框、圆角、选中/禁用/悬停；Theme 负责 QPainter 的同源颜色。Qt QSS 不原生支持 CSS 变量，可用 Theme 读取模板后替换占位符。
- 图标用 SVG 资源，避免 emoji 导致各平台风格不一致；字体先选系统可用中文字体，不依赖个人电脑上的特殊字体。
- 切页仅改变展示，不自动停流或销毁服务；是否暂停检测由明确的操作和产品约定决定。P1 隐藏时可以暂停视觉刷新。

## 5. 六个页面的实现

### P1 实时检测：RealtimePage

**对照设计稿**：左侧大视频，画面顶端多个角度标签和一个重点角度浮层；右侧是角度实时值、检测参数和动作提示；底部是停止检测、开始录制与延迟信息。

**布局**：页面主体 `QHBoxLayout`，VideoView 为伸缩区域，右侧卡片列初始宽约 280；底部单独放控制栏。HUD 作为 VideoView 子组件或由同一绘制流程生成；不要让它影响视频布局。

**操作与数据**：

1. 无设备时显示空态和“前往设备连接”，禁用检测/录制。
2. 设备就绪后启动检测；控制器协调 `startStream()` 和 `processFrame()`。
3. 视频、骨架和角度显示同一时间基准的数据，重点关节可由用户选择。
4. “骨架叠加/角度标注”只控制显示；置信度、平滑参数才影响推理模块。
5. 录制要等待服务确认成功后才点亮 REC 和开始计时；停止后确认 MP4 与元数据已完成写入，再保存会话并刷新 P4。
6. 动作提示使用分析模块提供的提示和等级；不要在 QWidget 中另写动作评判规则。

**刷新**：VideoView 使用 `update()`，由 GUI 线程绘制；高频数据先保存最新值，定时器以不高于目标刷新率取最新帧；文本与统计可用较低频率刷新。30fps 是目标，不能由定时器设置直接保证。

**验收**：视频不变形；缩放后骨架对齐；断流有状态；重复点击不会启动两次；录制失败不会显示成功；真实吞吐和延迟有测量依据。

### P2 设备连接：DevicePage

**对照设计稿**：左侧大配对二维码与三步说明；右侧上方设备列表，下方分辨率/帧率/延迟/电量和断开、刷新按钮。

**实现**：

- 左侧二维码先用样例占位；真实 URL/token 必须由采集模块提供。界面显示二维码、过期倒计时与重新生成入口。
- 右侧设备数量较少时用 `QScrollArea + QVBoxLayout + DeviceCard`；卡片按稳定设备 ID 更新，不按显示名称识别。
- “刷新”发出请求，立即显示忙碌状态；完成后更新列表。发现过程不得阻塞 GUI。
- “连接”进入 Connecting，重复点击禁用；收到 Connected 才更新侧栏、信息卡和 P1 可用状态。
- 显示 Disconnected/Connecting/Connected/Error；无法读取电量和延迟时显示“—”，不要填 0。
- 对当前设备断开时通知控制器停止相关流和录制，界面根据结果显示一致状态。

**缺口**：设计稿是手机扫描电脑二维码；现有 `grabFrameForQR()` 不能表达生成配对链接、会话过期和手机授权结果。需与孙梁确认完整流程，并补齐信号/接口。DeviceInfo 还缺设计稿里的地址、在线情况等字段。

### P3 检测配置：DetectionConfigPage

**对照设计稿**：顶部三选一模型卡；下方左侧是置信度/NMS/平滑参数与插值开关，右侧是关键点按钮；右下角恢复默认、保存配置。

**实现**：

- 三个 ModelCard 接入 `QButtonGroup`，确保单选；模型能力说明来自确认后的元数据，不能把设计稿“实时30fps”作为实测承诺。
- SliderField 组合标题、滑条、数值显示；用整数滑条映射浮点数，例如 `value / 100.0f`。各参数实际范围按模型能力确定。
- 关键点使用可勾选按钮；根据当前模型生成标签和 ID 映射。不能让 17 点与 33 点模型复用同一套 ID。
- `draftConfig` 保存未提交编辑；保存按钮发出完整配置，不在拖动每一格时重载模型。
- 控制器校验 → 异步应用模块参数 → 确认成功 → 持久化 → 同步其他页面。若持久化失败，应显示“已应用但未保存”，保留重试入口；若部分模块应用失败，应回滚或明确显示部分失败。
- 恢复默认先恢复草稿，再由用户保存应用；如团队选择“立即恢复并生效”，需在按钮和文档中写清楚。
- 加载中禁用重复提交；失败保留原有效配置，显示错误；保存后下次启动从 IDataManager 恢复。

**注意**：设计稿 P3 没有分辨率/帧率控件，P6 有；分工的 P3 验收却提到分辨率。建议按设计稿把采集参数放 P6，P3 管姿态参数，团队确认验收口径。

### P4 录像回放：PlaybackPage

**对照设计稿**：左上播放器，中间播放/暂停、进度、时间与倍速，左下角度曲线；右侧会话卡列表。

**实现**：

- SessionController 调用 IDataManager 获取会话列表和详情；SessionCard 只持有会话 ID 与摘要，不直接读文件。
- 选中会话后加载 MP4 及完整姿态序列；若只有角度序列，不能还原骨架。
- 复用 VideoView；播放工具栏发出 play/pause/seek/setRate 等用户意图，由孙梁提供的回放服务执行。
- 以播放器当前位置 `positionMs` 为唯一播放时钟，同步视频、骨架与 ChartView 游标。选择不晚于当前帧、且时间差在约定范围内的姿态；拖动后清除旧缓存并重新匹配。
- 用户正在拖动进度条时，暂时不让播放器周期更新覆盖手柄；释放时提交 seek。
- 以时间戳二分查找定位 PoseResult，不要每次从头扫描；倍率变化只改变播放时钟速度。
- 视频不存在、格式不支持、元数据丢失、无录像均需展示可理解状态。

**技术选择**：首选孙梁封装 IPlaybackSource，你只消费接口。如团队明确改用 Qt Multimedia，可由约定的负责人包装 `QMediaPlayer + QVideoSink` 输出到 VideoView，并补充 CMake 依赖与平台解码验证；不要同时维护两条默认回放链路。

**缺口**：ICameraSource 当前没有播放/暂停/拖动/倍速/时长接口；SessionRecord 当前只有 AngleSample，没有关键点历史。回放骨架必须先补数据契约。

### P5 数据分析：AnalysisPage

**对照设计稿**：顶部四个 KPI；中间左侧主曲线；右侧评分环与历史对比；右下导出报告、CSV。

**实现**：

- 顶部 KPI 用 CardWidget + QLabel 组成，先不为四个简单文本框各建一个类。
- 复用 ChartView 画膝角和躯干角；P4/P5 复用坐标系、图例、游标逻辑，显示的数据系列可不同。
- ScoreRing 用 QPainter 画背景圆弧、得分圆弧、中心分数/等级。明确评分范围，弧长按有效值除以满分计算；缺值显示“—”，不是 0 分。
- 历史对比接收后台提供的会话摘要和指标；比较基准与当前选择保持一致，避免拿错上一次会话。
- 点击导出先选择路径和格式，再在后台调用分析/数据接口；报告返回 QByteArray 时由约定层写文件，可用 QSaveFile 保证完整替换。
- 完成信号返回实际路径；只有写入成功才提示成功，并可用 `QDesktopServices::openUrl(QUrl::fromLocalFile(path))` 打开。
- 切换会话时给异步请求携带会话 ID 或请求版本，丢弃过期结果，避免旧分析覆盖新会话。

**缺口**：设计稿评分为 82（看起来按百分制），分工要求 0~10；设计稿显示“平均下蹲角速度”，类型给的是“峰值角速度”。团队必须统一指标和单位，不能仅更换标签。动作建议和历史增量也需要明确数据来源。

### P6 系统设置：SettingsPage

**对照设计稿**：左侧通用、摄像头与推流；右侧模型与推理、数据与存储、关于。

**实现**：

- 使用两列布局，内部五个 CardWidget，外部 QScrollArea。
- ToggleField/SliderField、QComboBox、QSpinBox 与文件夹选择器组合设置行。
- 统一字段命名，例如 `ui.language`、`camera.resolution`、`camera.fps`、`pose.modelName`、`inference.threadCount`、`storage.recordingDir`；版本化 schema 由团队确认。
- 通用外观由 UI 层应用；采集参数发给 Device/DetectionController；推理选项发给姿态模块；存储路径交给 IDataManager。
- 开关可即时提交；路径、分辨率、模型等组合参数完整校验后应用；避免控件刚初始化就触发一次保存，可使用 QSignalBlocker。
- 改变视频尺寸、协议等可能需要重建流；录制中可禁用相关修改，或明确提示停止后应用。
- “开机启动”“检查更新”涉及操作系统或更新服务，当前四个接口没有这些能力。第一版未实现时显示不可用并说明原因，不能只保存一个开关就声称生效。
- 推理设备列表和 GPU 可用性必须来自真实后端能力；设计稿 Apple M2 GPU 是样例，不能硬编码为实际设备。
- CSV/JSON 是结构化数据格式，MP4 是视频容器；建议明确两者的设置语义，避免“角度数据导出格式=MP4”。JSON 导出目前也没有接口。

## 6. 复用组件的具体职责

| 组件 | 输入与实现 | 不应承担 |
| --- | --- | --- |
| VideoView | 已拥有有效生命周期的 QImage、匹配 PoseResult、显示选项；paintEvent 绘制视频再叠加骨架；暴露实际视频矩形 | 采集、模型推理、MP4 解码 |
| AngleHUD | 已计算角度、当前重点关节、显示单位；画顶部角度标签与重点浮层 | 计算关节角度 |
| ChartView | 时间和值的序列、颜色、当前时间；画坐标轴、折线、图例和游标 | 计算评分、ROM、峰值 |
| ScoreRing | 有效得分、满分、等级和文案；drawArc 绘制 | 生成得分或判级 |
| CardWidget | 标题区与内容布局、统一圆角和背景 | 成为万能业务容器 |
| DeviceCard | DeviceInfo、连接中状态；发出 connectRequested(deviceId) | 连接相机 |
| SessionCard | 会话摘要、选中状态；发出 selected(sessionId) | 加载数据库与视频 |
| ModelCard | 模型描述、能力和选中状态；发出选中意图 | 加载模型 |
| SliderField | 名称、范围、步长、数值、显示格式 | 保存全局配置 |
| ToggleField | 标签、说明、开关值，提供可访问文本 | 直接操作 SDK |

ChartView 第一版建议使用 QPainter：当前只需几条折线和游标，减少依赖。数据量大时按可见时间窗和像素宽度进行保留峰谷的显示降采样；分析仍使用完整数据。只有出现复杂缩放、选区等需求时再评估 QCustomPlot，并核对其许可证和打包要求。

SkeletonPainter 由张在庆提供。你负责调用位置、裁剪区域、视频坐标转换和显示开关；Mock 阶段可有简化绘制替身，但真实接入时保留唯一骨架拓扑来源。

## 7. 视频显示中必须先解决的四件事

### 7.1 等比例适配与骨架坐标

对于已经统一好方向的视频，源尺寸 `(W,H)`、VideoView 可用区域 `(w,h)`：

```text
s  = min(w / W, h / H)
dw = W * s; dh = H * s
ox = (w - dw) / 2; oy = (h - dh) / 2
videoRect = (ox, oy, dw, dh)

归一化点 (x,y) → (ox + x*dw, oy + y*dh)
```

骨架按真实 `videoRect` 绘制，不能按整个控件区域。镜像、手机旋转、裁剪必须让视频和关键点使用同一变换。若选择铺满裁剪，需另算裁剪变换，不能继续使用上面的完整适配公式。

### 7.2 帧与结果匹配

推理通常晚于视频到达。每个帧/结果必须有明确时间单位、时钟和 session/stream 标识；建议加入 frameId。可采取小容量缓存并以时间戳匹配，在可接受延迟内显示匹配帧；超时可以显示纯视频或标记姿态过期。不能无限等待，也不能把旧姿态无条件画在最新帧上。

### 7.3 内存生命周期

`const cv::Mat&` 信号不等于零复制或线程安全。cv::Mat 通常仅浅复制引用计数；SDK 若复用底层缓冲，接收线程可能读到变化中的内容。

团队应明确使用不可变的共享帧及其拥有者，或有生命周期保证的帧池。若无法保证，就在边界做必要深复制。BGR→QImage 同样要处理颜色顺序、stride 和所有者；不能让 QImage 指向已释放/被复用的临时 Mat 内存。FrameImageAdapter 是唯一转换入口，避免每页重复转换。

### 7.4 队列与 GUI 线程

- 采集、推理、分析和耗时 I/O 放后台；QWidget 只在主线程更新。
- Qt queued signal 本身不会限制队列长度。采用有界 mailbox/环形缓冲，信号只通知“有新数据”；显示消费最新数据，推理忙时按规则替换旧待处理帧。
- 显示队列、推理队列、录制队列分开。显示可以丢旧帧，录制需要独立的吞吐、写入错误和背压策略，不能共用“只留最新一帧”。
- 跨线程自定义类型需声明并注册 Qt 元类型；这只解决传递机制，不能解决内存所有权。
- 跨线程直接调用 QObject 成员函数不会自动切换到其线程；queued 调用不能依靠原有 bool 返回值同步确认结果。需要 request/completed/failed 信号或明确的异步适配器。

## 8. 对现有接口的补充清单

以下是联调前的提议，不要由 UI 单方面修改公共头文件。

| 缺口 | 影响 | 建议与负责人确认 |
| --- | --- | --- |
| ConnectionState 未完整定义、SessionMetrics 定义顺序 | 公共头文件无法直接作为完整可编译契约 | 先声明枚举；SessionMetrics 定义在按值使用它的 SessionRecord 前 |
| IAnalysisService/IDataManager 无虚析构 | 经基类指针销毁派生实现可能出错 | 添加 `virtual ~...() = default`；QObject 接口也按统一规则声明 |
| 阻塞方法与 GUI 不兼容 | 发现、模型加载、分析、导出卡住页面 | 约定工作线程、请求 ID、完成/失败通知；同步接口由异步适配层包装 |
| 回放控制不存在 | P4 无法真正工作 | 孙梁提供 IPlaybackSource，或团队确认 Qt Multimedia 包装归属 |
| SessionRecord 只有 angles | 回放无法恢复关键点与骨架 | 存完整 PoseResult 或姿态文件路径，并定义时间戳、模型、拓扑和版本 |
| 录制完成事件和元数据输入不存在 | 停止后无法确认文件可用，也无法将推理结果入库 | 定义 recordingStarted/Finished/Failed 与 PoseResult 录制提交路径，明确负责人 |
| 二维码方向与契约不一致 | P2 真配对流程无法实现 | 孙梁定义电脑生成配对链接、有效期、手机确认、连接结果 |
| Keypoint 仅 id，模型有 17/33 点 | ID、连线和选择项混淆 | 张在庆提供模型元数据：名称、ID、拓扑、支持参数和角度依赖 |
| 提示与告警缺数据源 | P1/P5 只能写死文案 | 陈艺夫提供提示结构、级别和实时/离线获取方式 |
| 分数和角速度语义不一致 | 数值正确但展示含义错误 | 统一 0~10/0~100、平均/峰值、角度单位、缺值、对称性范围 |
| listSessions 仅 ID，无时长、状态 | P4 列表展示不完整 | 第一版逐项 load；后续增加摘要/分页，避免列表加载全部序列 |
| 设备状态变化但无信息更新事件 | 帧率/延迟/电量无法持续刷新 | 提供 deviceInfoChanged 或低频异步查询 |
| P6 多项能力未暴露 | 保存了值但后端未生效 | 补能力查询/应用接口；未实现项明确禁用 |

## 9. Mock 先行

由 main.cpp 根据启动参数选择服务，例如建议 `--mock`；同一组页面和控制器消费相同 I* 接口。

- MockCameraSource：固定设备列表、连接延迟、模拟断流/失败；使用合法样例帧。不要把设计稿视频背景当作你的真实测试结果。
- MockPoseEstimator：生成随时间变化的点和角度，保持模型 ID 一致。合成数据只用于展示与交互，不作为模型效果证据。
- MockDataManager：提供固定会话、配置与历史；要验证“重启持久化”，Mock 也需使用临时 JSON/测试目录存储，纯内存 Mock 无法验证。
- MockAnalysisService：返回明确标注的样例指标和报告，支持延迟与错误注入。
- MockPlaybackSource：产生可拖动的时间轴、时长和帧事件，用完整样例姿态驱动骨架与游标。

UI 阶段应覆盖无设备、无数据、加载中、连接失败、录制失败、导出失败等状态；真实模块准备好后只替换装配，不修改页面。

## 10. CMake 与工程迁移

1. 保留现有 `main.cpp`、`mainwindow.*` 入口，先在现有 MainWindow 中完成外壳，不同时做大规模移动。
2. 开启/保持 AUTOMOC、AUTOUIC、AUTORCC；当前 `qt_standard_project_setup()` 已提供部分标准设置，资源编译配置需按项目确认。
3. 将 `modules/ui` 建成 `motion_ui` 静态库，源码显式列出，链接 Qt6::Widgets 与公共契约 target；页面头文件避免直接 include 真实 SDK。
4. 公共契约若暴露 cv::Mat，则契约/适配目标仍需正确传递 OpenCV 头文件依赖；Mock 不等于整个工程完全无需 OpenCV。
5. 真实模块目标由其他负责人提供；不要假定这些 CMake target 已经存在。GUI 使用 QPainter 图表无需额外 Qt Charts。
6. 只有选择 Qt Multimedia 回放时才加入对应组件。若使用 SVG 图标，确认部署包含相应图像插件/Qt Svg 能力。
7. 静态库中的 qrc 可能需要显式资源初始化或可执行文件侧加入资源，确保发布包也能加载 QSS/图标；不能依赖开发机绝对路径。
8. 建议使用 C++17，源码放源码目录，构建产物放独立 build 目录；Qt Creator 中继续打开原 CMakeLists.txt。

## 11. 推荐实施顺序

| 阶段 | 先创建/实现 | 可检查的结果 |
| --- | --- | --- |
| 1 外壳 | MainWindow、Sidebar、TopBar、PageId、六个空页面、Style.qss/qrc | 六页切换、标题和高亮一致，1440×900 基本比例正确 |
| 2 静态组件 | CardWidget、ToggleField、SliderField、ModelCard；先做 P3/P6 | 展示接近设计稿，草稿/选中/禁用状态正常 |
| 3 Mock 实时链路 | VideoView、AngleHUD、FrameImageAdapter、MockCamera/Pose、DetectionController | 无真实相机也能演示视频+骨架+角度+控制状态 |
| 4 设备和配置 | P2、DeviceController、SettingsController、配置 Mock | 连接/断开/失败可演示；P3/P6/P1 数据一致；重启能恢复配置 |
| 5 回放和分析 | ChartView、ScoreRing、SessionCard、SessionController、P4/P5 | 会话切换、seek、游标、评分与导出完整联动 |
| 6 真实接入 | 逐个替换 Mock，验证线程/时间戳/元数据 | 实际设备到 P1、录制到 P4、分析到 P5 的完整流程 |

第一阶段不需要一次创建目录树里的全部类。先完成“能编译、六页可切”的可见成果，再增加抽象。

## 12. 验收与风险

- 在 1440×900、较小窗口、高 DPI 下对照六页截图检查布局；文字、焦点、滚动与禁用状态可用。
- 视频以已知位置的样例点验证缩放、留黑边、镜像、旋转；分别验证实时与 seek 后的时间戳匹配。
- 设备连接失败、模型失败、配置保存失败、文件写入失败均必须反馈；异步旧请求不能覆盖新会话。
- 配置保存后重启验证，不能只看一次 success 提示。
- 导出必须检查实际文件存在、格式正确且能打开；Mock 导出与真实报告验收分开记录。
- 实测帧率、端到端延迟、长时间运行内存与录制完整性；分工中的 25/30fps 是验收目标，不是本方案已达到的性能。
- 主动停止采集/推理/录制并等待必要的完成事件后退出，避免截断录像文件。

目前已确认的是分工、设计稿布局和空壳代码状态；没有实际实现六页，没有编译新架构，也没有运行真实采集/推理/分析。本方案不将接口提议或 Mock 数据表述为现成功能。
