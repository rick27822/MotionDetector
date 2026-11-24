# 第二周个人模块测试报告

验收日期：2026-11-24。分支：week02-analysis。提交范围为陈艺夫个人分析、测试和文档。

## 环境与测试

Windows x64，MSVC19.44.35217，C++17，CMake/Ninja Release，/W4 /WX /EHsc /utf-8。项目面向Qt6.5.3，本次个人算法目标为纯C++，不需要链接Qt或队友模块。

4/4组CTest通过：analysis_mock_contract（17条）、offline_analysis（28条）、camera_time_contract（11条）、offline_quality_rules（43条）。完整证据见[运行日志](week02-validation.log)。计数为测试程序断言数，不代表代码覆盖率。

覆盖左膝极值、ROM、速度、计次、有效位、双膝ROM比值、评分权重、A/B/C等级、规则版本、缺失数据、重复时间、长间隔、阈值精确边界、巨大有限权重及溢出总权重。

12次合成深蹲演示为49点，左膝90–170°，ROM80°，峰值速度250°/s，计次12，对称性1。默认评分不可用；实验演示得到9.44444/10、A。测试中的已知理想规则样例为10/10，非使用相同目标ROM的演示值，两者分开验证。

## 规则结论

第二周离线模块技术验收通过。站立≥160°、蹲底≤120°、周期≥500ms、间隔>500ms中断，边界测试通过。评分采用ROM/对称性/躯干50%/30%/20%加权，默认禁用，版本和参数须由调用方提供。参见[规则说明](week02-scoring-proposal.md)。

技术验收不代替团队对评分标准的共同确认，也不包含真实姿态识别精度、UI、相机桥接、业务数据库和正式导出验收。本次保留原有core/types.h，不提交张同学的源码、公共类型更新或新姿态样例。

## 复现

在VS x64开发者终端从项目根目录运行：

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build\offline_demo.exe
build\offline_demo.exe --experimental-score
```
