# 给陈艺夫的 UI 对接回复

> 历史回复快照：下文“尚无Git分支”描述写于上传之前。当前源码已整理在 `feat/chenkerui-ui-stage1` 分支；其余对接提案仍需共同确认。

以下可直接转发。标注“建议/拟新增”的部分是联调提案，不是当前代码已具备的接口，也尚未经过双方确认。

我这边六页Qt UI的阶段一已经完成，当前是可独立运行的样例版。最新源码我打成了 `MotionDetector-UI-Stage1-src.zip`，暂时还没有推送到Git分支，因此目前没有可直接拉取的分支名。源码包里包含六页界面、控制器、姿态样例解析和M0适配。

本机验证环境为macOS arm64、Qt6.11.0、C++17、CMake3.21+。解压后用Qt Creator打开 `motiondetecter/CMakeLists.txt` 并选择Qt6桌面Kit。Windows尚未验证，不需要照抄Mac的Qt路径。

我当前接入的仍是你之前的M0 Mock。你这次跑通的离线分析和姿态信号接入版本，请把最新源码、接口和验证命令一并发我，接下来按真实版本适配。

## 1. poseReady 接线

当前样例版没有真实PoseEngine，尚无正式poseReady接线。建议由主窗口/应用装配层连接 `pose::IPoseEstimator::poseReady(const core::PoseResult&)` 到拟新增的 `DetectionController::onPoseReady(const core::PoseResult&)`。

控制器负责将同一份姿态数据分发给P1显示、实时动作提示和录制会话收集；六个页面不各自连接或重算角度。耗时分析由工作线程调用，结果附会话ID/请求版本，避免切换会话后被旧结果覆盖。

输入以姿态包的core::PoseResult/core::AngleSample为主。若你保留motion::类型，我这边已有AnalysisM0Adapter作转换，不必为了UI强行重写算法。两套同名core/types.h不能直接互相覆盖。

## 2. 会话ID与录制起点

建议由UI应用层统一管理一次录制的生命周期，SessionController协调会话，DetectionController负责开始/停止请求。当前样例ID是QString/UUID，正式联调建议沿用不透明recordingId；如果数据库用qint64主键，可另存并建立映射，不把UUID强转整数。

录制起点必须取相机/录制模块确认写入的第一帧单调时间T0，不取按钮点击时刻，也不取晚到的第一条poseReady。收集到姿态后按 `sample.t = pose.timestampMs - T0` 转为会话相对毫秒；T0之前的样本不纳入。采集重启/换源需另起stream generation，防止时间戳重置后串流。

这个起点和视频PTS/帧映射需要与孙梁共同确定；Unix秒的received_at不能直接和单调毫秒相减。视频编码与姿态元数据完成后，再由你的数据管理接口完成会话入库。

以上是正式联调建议；当前UI“样例录制”仅保存合成角度片段，不生成MP4。

## 3. P1 动作提示

建议一次返回一条最高优先级提示，UI只渲染，不在页面中实现动作判断规则。最小字段建议为：

| 字段 | 用途 |
| --- | --- |
| timestampMs | 本次提示所对应的姿态时间，与PoseResult同一时间基准 |
| code | 稳定的提示代码，方便区分无姿态、蹲深、躯干等规则 |
| severity | info/warning/error或双方约定的枚举，用于卡片配色 |
| message | 最终展示的中文文案 |
| valid | 是否有当前有效提示；false时清除旧提示 |
| simulated | 是否模拟数据，UI显示来源标记 |
| focusAngle（可选） | 相关JointAngle，供HUD高亮 |
| validForMs（可选） | 过期时间，避免断流后保留旧告警 |
| ruleVersion（建议） | 规则版本，便于会话与报告追溯 |

无人体、无有效膝角、规则不适用时需要明确状态，不能把无效角度当0°判断；0°对躯干可能合法。角度已经在姿态侧平滑，UI不再次低通滤波。

## 4. P5展示字段

当前UI展示契约在 `contracts/IAnalysisProvider.h`；AnalysisM0Adapter仅负责把服务输出转换成显示模型。

| 显示项目 | 当前UI字段 | 语义 |
| --- | --- | --- |
| 最大膝角 | maximum | 度；请确认左右膝的汇总规则 |
| 活动度ROM | rom | 度；请确认统计对象及有效区间 |
| 峰值角速度 | peakSpeed | °/s；当前已改为峰值，不使用设计稿中的“平均”冒充 |
| 左右对称性 | symmetryPercent | UI展示0~100%；若服务返回0~1，在适配层乘100一次 |
| 综合评分 | scorePercent | UI展示0~100；若服务仍返回0~10，在适配层乘10 |
| 等级 | grade | 使用服务生成的最终中文等级 |
| 结果状态 | valid / simulated / error | 区分真实、样例、无数据与失败 |
| 主曲线 | QVector<core::AngleSample> | 需要左膝与躯干通道；t为会话相对毫秒，保留valid掩码 |

真实版本还希望提供会话ID、规则版本、有效样本数，以及各指标是否有效的状态，避免局部指标缺失时误显示0。

历史对比需要会话ID/名称/时间和与当前一致定义的膝角指标；目前UI历史条是视觉样例，待替换。评分建议文案由分析服务提供。导出先打通CSV和HTML即可，PDF若已实现也可一起接；返回成功/失败及实际文件路径，NotImplemented不能当成功。

## 5. P3/P6持久化清单

当前实际字段定义在 `app/AppState.h` 的UiConfig，序列化在SettingsController所在源文件中，schemaVersion=1。现阶段由UI本地JSON保存，正式接你的IDataManager后使用同一个配置入口，避免P3/P6各存一份。

| 页面 | 当前需要保存的字段 |
| --- | --- |
| P3 | confidence、smoothing（EMA alpha，越小越平滑）、interpolate、enabledPoints |
| P6 | resolution、fps、threads、mirror、storagePath |
| 版本/保留字段 | schemaVersion；nose目前有存储字段但控件禁用 |

enabledPoints当前保存中文展示名，正式schema建议改用姿态包的稳定关键点ID或mask，我这边做旧配置迁移。不能拿不同模型的原始索引直接混用。

model/backend、nmsThreshold、GPU、推流协议、弱网降级、开机启动、检查更新、JSON导出等目前没有作为已生效功能开放。先保留扩展位，等对应服务能力确认后再增加，避免只存值却没有真正生效。P1的骨架/角度显示开关当前是运行时状态；若统一做UI偏好持久化，也可以放入单独ui配置区。

保存流程建议区分“参数应用成功”和“持久化成功”；失败保留原配置或明确显示已应用未保存，加载配置时不触发重复保存。

你先把最新分析/数据接口和接入示例给我，我们优先打通：姿态输入→会话角度采集→离线分析→P5展示→CSV/报告导出。会话时钟与视频映射再和孙梁一起冻结。
