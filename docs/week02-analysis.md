# 第二周离线分析（当前实现子集）

前置：第一周M0接口和Mock已通过PR合入chenyifu；本分支week02-analysis从该合并点创建。

## 本次新增

- modules/analysis/AnalysisEngine.h：左膝极值、ROM、峰值角速度和基础周期。
- modules/analysis/CameraTimeAdapter.h：相机会话/帧身份及Unix秒到会话毫秒转换辅助。
- tests/analysis_tests.cpp：28项检查。
- tests/camera_time_tests.cpp：11项检查。
- examples/offline_demo.cpp、week2.pro及相关构建入口。
- validation/qt_smoke：Qt Widgets/SQLite/分析引擎集成验证源码。
- docs中的相机接口来源、对接记录和验证报告。

## 第一周内容继续保留

core/types.h、两个I*接口、MockServices、mock_demo、17项Mock检查、week1.pro及docs/week01-m0.md。

本周根CMakeLists扩展为三组测试，README更新本机Windows与队友macOS差异；并未另建一套嵌套项目。

## 仍未完成

综合评分、左右对称性、真正动作纠正、业务数据库、CSV/报告导出、真实Python相机到C++的通信、完整UI。不能把基础离线测试通过表述为全部M1验收完成。

本次暂不提交.github工作流；多复制的工作流已备份到聊天工作目录，后续单独配置CI。

## 提交说明建议

feat: add week2 offline analysis and tests

PR目标chenyifu，来源week02-analysis。先核对Desktop的Changes，再手动提交和推送。
