# 阶段一集成边界

> 2026-11-26 更新：本文件描述阶段一基线。第三周已接入有状态实时提示服务，新增文件输入与真实姿态公开接收入口，详见 [P1实时提示联调交接](P1实时提示联调交接.md)。真实采集/推理装配与视频链路仍未接入。

## 已接入的交付

- 复用姿态包的 `core/types.h/.cpp`、SkeletonPainter。未编译PoseEngine、ONNX推理或Camera模块。
- PoseSampleReader读取交接的三帧JSON与382帧JSONL，兼容中文/英文角度键；保留valid与合法0°。冲突、无效掩码、错误时间序列会报错，失败不会替换调用方原有结果。
- AnalysisM0Adapter将core角度转换成motion七路输入，调用实际M0 MockAnalysisService。显示分数为qualityScore×10，对称性为symmetryScore×100，且持续标记模拟。
- M0私有类型改名为M0Types.h，接口include改为明确相对路径。原namespace和Mock算法未更改，不将两个core/types.h搜索路径混用。

## 阶段一演示服务

AppState保存共享状态；SettingsController用QSaveFile保存版本化UI配置，SessionController保存演示会话元信息并读取内置样例。它们不冒充正式IDataManager。CSV/HTML是UI演示导出器的实际文件输出，与M0的NotImplemented导出能力区分。

P1默认画面来自现有AI生成人像图片，骨架为人工视觉标注；选中交接样例后在无视频背景上绘制对应骨架和角度。P4默认画面和骨架均为静态参考展示；播放器控件只驱动角度样例时间轴。382帧JSONL不含关键点，不能产生连续姿态回放。

## 下一阶段需要替换/新增

1. Python相机桥接/设备服务：进程通信、真实帧、配对与网络、单调时钟、session/sourceFrameId、颜色/镜像/旋转与变换。
2. 姿态服务：Mac兼容路径与dylib、真实模型/后端就绪事件、限流、时间关联与异常关闭。当前PoseModule原ONNX Mac问题仍不在本阶段内修复。
3. 录制与回放服务：真实MP4、编码完成、完整PoseResult历史与视频PTS映射、Qt Multimedia或团队解码器。
4. 数据/分析服务：真实持久化、分析规则与等级、历史指标、CSV/报告服务。IAnalysisProvider为现有分析替换入口；其他服务契约需与负责人共同冻结。
5. 当前UI配置是样例配置，并未向设备或推理引擎下发。真实接入后必须增加apply完成/失败与保存的独立状态。
