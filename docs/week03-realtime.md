# 第三周 M2：陈艺夫实时动作提示

分支week03-realtime，从已合并第二周成果的chenyifu创建。沿用modules/analysis、tests、examples、docs及week3.pro结构，仅新增个人模块。

## 本轮个人实现

IRealtimeAnalysisService为独立逐帧接口，RealtimeAnalysisService实现准备/下蹲/蹲底/起身/完成/不完整动作/数据丢失提示。输出累计次数及单帧完成事件。RealtimePoseAdapter提供团队姿态对象到本服务输入的薄转换，无需更改任何版本的core/types.h或旧IAnalysisService。

每帧时间和内存开销不随会话长度增长，不保存无限历史，也不重复扫描全部录像。实例由一个分析线程持有，不支持多个线程同时调用。

默认使用第二周计次工程阈值：站立≥160°、蹲底≤120°、周期≥500ms，间隔>500ms中断未完成动作。中间阶段按相邻帧角度变化判断起身/下蹲，变化不足2°保持上次方向。此处没有另外平滑姿态数据，关键点和角度滤波由姿态模块负责。

未就绪时必须先站立。缺失左膝或数据超时不补计次数。过程中的丢帧、重复/倒序输入中断当前动作，但保留已完成次数；会话变更或reset清空次数。未知frameId=-1允许，已知帧号需递增。时间必须非负且严格递增。

可选躯干前倾阈值提示默认关闭。启用后合法躯干角>45°时forwardLean=true，等于45°为false，躯干缺失时trunkHintAvailable=false。这是工程阈值提示，不输出“动作正确”或综合评分。

## 已验证

VS/MSVC19.44、C++17、Release构建。5组CTest通过，其中41条实时断言。覆盖动作阶段、12次连续计数与离线一致、时间/帧号异常、无效角度、500ms超时边界、会话切换、完成事件不重复、可选躯干提示、适配器合法0°及无人体时清除旧角度。原有四组回归继续通过。

演示已实际运行，输出0ms准备、200ms下蹲、400ms蹲底、600ms起身、800ms完成一次、1400ms数据中断。证据见[测试日志](week03-validation.log)。

适配器测试使用契约形状Mock，未将队友的源码复制到个人分支。当前完成个人模块自测，尚未完成真实Qt P1联合验收。

## 下一步联合验收

陈柯睿在P1控制器接入poseReady，显示提示与次数，按时检查无新帧超时。张在庆确认真实PoseResult角度/有效位/时间与本适配器匹配。需要三人一起运行P1真实帧+骨架+HUD+动作提示，验证停止/断线/重连及线程运行情况，才能认定第三周M2全链路完成。

具体接线见[交接说明](week03-ui-handoff.md)。本轮不推进数据库、报告或P5修改。

## 构建

VS x64开发者终端执行cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release，然后cmake --build build、ctest --test-dir build --output-on-failure。独立演示为build/realtime_demo.exe。Qt Creator/qmake入口为week3.pro。
