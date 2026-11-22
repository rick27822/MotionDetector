# PoseLab · 动作捕捉检测系统

源码分支：`feat/chenkerui-ui-stage1`。本分支为陈柯睿负责的六页 UI 阶段一成果，基于团队 main 初始化提交。相机和分析的独立分支尚未合入本分支。

当前交付：**陈柯睿阶段一：UI 可独立验收**。六页 Qt Widgets 界面、统一深色主题、导航与样例交互已实现。真实手机连接、真实模型推理、MP4 录制/解码及真实动作评分属于下一阶段。

## 直接运行

在 Finder 中双击 `output/PoseLab-UI.app`。这是当前 arm64 Mac 的本地验收包，带 Qt 运行依赖，不需要 Python、OpenCV 或 ONNX 模型。应用采用本地 ad-hoc 签名，尚未进行 Developer ID 公证；当前已验证平台为 macOS 15.5 / arm64，不承诺其他系统版本。

也可以在工作区终端运行：

```sh
open output/PoseLab-UI.app
```

右上角 **UI 样例 ▾** 可以切换正常、无设备、无会话、连接失败、配置保存失败、录制失败、导出失败场景。失败开关用于检查对应操作的反馈，切回“正常样例”即可恢复。

## 六页如何验收

| 页面 | 可以操作的内容 | 数据边界 |
| --- | --- | --- |
| 实时检测 | 开始/停止、骨架与角度开关、镜像、完整/裁剪画面、样例录制、姿态样例切换 | 默认是静态图片+人工视觉标注；站立/蹲底/起身来自交接JSON，在无对应视频的独立背景显示 |
| 设备连接 | 刷新、连接两台在线样例设备、断开、加载/失败/重试 | 没有真实网络连接；二维码区是明确不可扫描的占位 |
| 检测配置 | 置信度/EMA草稿、插值开关、12个身体关键点、默认值、保存 | 参数实际持久化，关键点选择影响样例叠加；未执行真实推理，未支持模型/NMS禁用 |
| 录像回放 | 样例会话选择、时间轴播放/暂停、拖动、点击曲线定位、0.5/1/1.5/2倍速 | 静态参考画面和人工标注不随时间轴运动；只有角度样例，无MP4解码和连续骨架回放 |
| 数据分析 | 样例曲线、M0模拟指标、百分制评分环、CSV/HTML样例导出、错误反馈 | 曲线来自382帧合成序列；指标来自固定M0 Mock；历史条是明确标注的视觉样例 |
| 系统设置 | 分辨率/帧率/线程数样例参数、镜像、目录选择、自动保存 | 保存本机UI配置；真实推流、GPU、更新、开机启动等能力禁用 |

配置默认写入 Qt 的 `AppLocalDataLocation`，在当前 Mac 通常为 `~/Library/Application Support/MotionDetector/PoseLab-UI/`。可用 `--data-dir` 指定独立验收目录。保存前的P3编辑只影响草稿；保存成功后同步P1/P6；损坏配置不会被启动过程覆盖。

“样例录制”保存的是最多12.57秒的合成角度片段和会话元信息，不产生视频。新增样例会话可跨重启保留。系统设置中的样例文件保存路径控制导出对话框的默认目录；应用配置和会话索引仍由data-dir管理。

样例CSV采用UTF-8 BOM，第一行是标准表头，每条数据含 `source=synthetic-ui-sample`。HTML报告显著注明合成数据与固定Mock指标。它们由UI演示导出器生成，不代表陈艺夫正式导出服务已完成。

## 查看视觉和测试证据

- [六页原生 Mac 截图总览](docs/verification/ui-stage1/six-pages.png)
- [设计稿/界面透明度对照页](docs/verification/ui-stage1/comparison/index.html)（用浏览器打开）
- [原生 Mac 六页截图与控件坐标](docs/verification/ui-stage1/native-screenshots/)
- [加载、失败与空状态截图](docs/verification/ui-stage1/states/)
- [自动测试完整输出](docs/verification/ui-stage1/tests.txt)
- [打包记录](docs/verification/ui-stage1/package-log.txt)
- [样例CSV](docs/verification/ui-stage1/exports/sample-angles.csv)、[样例HTML报告](docs/verification/ui-stage1/exports/sample-report.html)
- [阶段一验收记录与限制](docs/verification/ui-stage1/验收记录.md)

对照1440×900设计稿实现了主框架、布局、颜色、卡片、按钮和曲线风格。没有设计源字体，字形与光栅化可能不同。必要的状态/数据语义已调整；不宣称逐像素一致，也不用像素分数替代人工检查。

## 从源码构建

已验证：AppleClang 17、C++17、CMake、Homebrew Qt 6.11.0。保留Qt6.5最低API要求，但当前执行环境中Qt6.5.3的rcc/uic启动报NEON检测错误，因此实际构建与测试使用6.11.0。

```sh
cmake -S motiondetecter -B motiondetecter/build-ui-qt611 \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build motiondetecter/build-ui-qt611 -j 4
ctest --test-dir motiondetecter/build-ui-qt611 --output-on-failure
```

Qt Creator 打开 `motiondetecter/CMakeLists.txt`，选择与上述一致的Qt套件。页面布局使用C++和Qt布局管理器构建，便于共用组件、设置稳定objectName并运行确定性测试；原有空白 `mainwindow.ui` 已不参与构建，不要编辑它来修改当前页面。

重新打包（脚本会先检查暂存包，`--replace`只替换生成的应用包）：

```sh
python3 motiondetecter/tools/package_macos.py --replace
```

打包脚本针对本机Homebrew布局，复制必需的Cocoa/offscreen插件、Qt框架与依赖，检查外部动态库引用并执行本地ad-hoc签名。

## 自动截图与验收数据

以下命令均从工作区根目录运行。截图/导出命令默认使用临时数据目录，不覆盖正常运行配置。

```sh
QT_QPA_PLATFORM=offscreen \
  output/PoseLab-UI.app/Contents/MacOS/motiondetecter \
  --capture docs/verification/ui-stage1/screenshots

QT_QPA_PLATFORM=offscreen \
  output/PoseLab-UI.app/Contents/MacOS/motiondetecter \
  --export-samples docs/verification/ui-stage1/exports

POSELAB_EVIDENCE_DIR="$PWD/docs/verification/ui-stage1/states" \
  QT_QPA_PLATFORM=offscreen motiondetecter/build-ui-qt611/ui_tests
```

`tools/compare_screenshots.py` 需要Pillow；可使用Codex已提供的Python运行时。`tools/capture_reference.py` 需要Poppler。应用本身和Qt测试不依赖这些Python工具。

## 主要源码入口

- `mainwindow.*`：主窗口、顶栏、路由、验收场景、控制器装配。
- `app/AppState.h`：共享状态与UI配置。
- `modules/ui/pages/`：六页实现。
- `modules/ui/widgets/`：CardWidget、ToggleField、SliderField/FlatSlider、ScoreRing、VideoView、ChartView。
- `modules/ui/controllers/`：设备、检测、会话、配置控制器；当前为独立UI演示实现。
- `modules/ui/adapters/`：中英文样例解析与M0分析适配。
- `contracts/IAnalysisProvider.h`：分析替换入口。
- `modules/ui/theme/`：样式与颜色/图标工具。
- `core/`、`modules/pose/SkeletonPainter.*`：复用张在庆交付的类型/绘图工具，未导入或运行ONNX后端。
- `vendor/analysis_m0/`：陈艺夫M0类型和Mock，私有头路径已隔离。

后续接真实模块时，需要新增相机桥接、视频解码/录制和真实数据服务适配器，并补齐就绪/失败通知与时间戳契约；不能仅删除“样例”文字就视为完成集成。
