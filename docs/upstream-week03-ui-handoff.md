# 第三周 P1 动作提示交接

陈艺夫提供modules/analysis/IRealtimeAnalysisService.h、RealtimeAnalysisService.h、RealtimePoseAdapter.h。它们只依赖C++17标准库，不包含旧core/types.h。陈柯睿负责控制器连接、线程调度和UI绘制。张在庆提供PoseResult，不改变推理或角度算法。

## 调用方式

以下为对接示意，需在Qt主工程中完成接线后联合验证。成员stream由一个线程持有，core类型来自UI主工程已有姿态头文件，禁止混入旧分析core/types.h。

```cpp
#include "modules/analysis/RealtimeAnalysisService.h"
#include "modules/analysis/RealtimePoseAdapter.h"

motion::RealtimeAnalysisService stream; // 控制器持有，不要每帧重建

// poseReady回调的接收线程中，currentSessionId由主程序提供。
auto input = motion::adaptRealtimePose<core::JointAngle>(pose, currentSessionId.toStdString());
auto hint = stream.processPose(input);
// 将hint.message / completedReps发送到UI线程显示。
// 只在hint.repCompleted==true时处理本次完成事件。
```

当前旧IAnalysisService::evaluateHint为原接口兼容方法，P1动态阶段和计数应使用新增实时服务。不要把两者当作同一有状态对象。

## 时间、会话和有效性

PoseResult没有统一sessionId时，由主程序显式提供稳定的当前采集/训练会话ID。时间沿用pose.timestampMs的同一会话单调毫秒，不能传Unix时间，也不使用录像起点替代。

按主程序统一单调时钟定期调用stream.poll(nowMs)。需要保证nowMs与pose.timestampMs处于同一时间轴；若没有统一时钟，可在接收帧时锚定该帧时间和本地elapsed计时器，做无新帧超时检查，但该办法无法测量帧到达前的排队延迟。

断开设备/停止检测后，先保存所需结果，再调用reset并清理P1提示。重新连接创建新会话ID或显式reset。会话ID改变会自动清空次数。

pose.detected=false时适配器强制角度无效，避免沿用上一帧角度。hasAngle决定有效性，0°是合法值。旧frameId=-1可用，真实frameId需按会话递增。迟到/重复帧拒绝并重置当前未完成动作。

## 提示显示

code表示状态，message为中文文案，completedReps为当前会话次数。repCompleted仅在完成的那次processPose返回中为true，poll不会重复发完成事件。DataLost/InvalidInput要及时覆盖旧的完成或下蹲文字。

trunkHintAvailable和forwardLean为独立可选提示，默认关闭。缺失躯干不能显示为“姿态佳”。此接口不返回实时质量分。

## 联合验收清单

- 正常动作一整次：P1提示按准备/下蹲/蹲底/起身/完成变化，次数只增加1。
- 连续12次：流式次数与离线分析一致。
- 中途丢失人体或超过500ms无帧：提示数据中断，不补计动作；恢复后先站立。
- 停止/断线/重连：提示和会话次数按约定清理，不显示旧会话数据。
- 张同学确认角度、有效位、帧号和时间定义；陈柯睿确认UI线程更新及输入队列不会积压。
- 在真实主程序测帧率、骨架/HUD/提示一致性。个人纯C++测试不等于P1联合验收或实时性能验证。
