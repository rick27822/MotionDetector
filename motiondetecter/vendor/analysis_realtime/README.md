# 陈艺夫第三周实时提示服务

三个头文件逐字复制自上游 `9d74c5e6fec2f39c380f41945ef9a7a4df984d30` 的 `modules/analysis/`，SHA-256 记录见 upstream.json。本次没有修改动作判断、计次阈值和角度算法。

此接口只依赖 C++17，不包含旧分析模块的 core/types.h。UI 以自身 core::PoseResult 实例化 adaptRealtimePose。旧 M0 离线分析仍位于 vendor/analysis_m0，二者独立。

完整原始交接说明见仓库 docs/upstream-week03-ui-handoff.md。后续更新服务时同步版本、哈希和主程序验证结果。
