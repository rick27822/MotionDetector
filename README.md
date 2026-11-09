# MotionDetector

动作捕捉检测系统。Qt桌面端由团队协作实现。

## 开发环境

- Qt 6.5.3、C++17。
- 陈艺夫本机：Windows x64、MSVC2022。
- 队友环境：macOS、Apple Clang17.0.0。Apple编译器不在Windows安装。

## 当前个人分支内容

第一周：接口和Mock，见docs/week01-m0.md。
第二周：左膝离线指标、基础周期识别、时间适配及测试，见docs/week02-analysis.md。

当前不是完整六页应用。公共类型仍待四人确认，评分、业务持久化、导出和真实相机桥接尚未完成。

## 构建与测试

在已配置MSVC的开发终端中：

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

qmake入口：第一周week1.pro、第二周week2.pro。当前中文路径下qmake/NMake存在编码问题，优先使用CMake+Ninja；若使用qmake，请选英文路径。

validation/qt_smoke提供Qt Widgets/Sql与真实分析引擎的小型集成检查，详见其README。完整校验范围见docs/校验报告.md。

在姓名分支chenyifu汇总，每周任务分支经PR合回姓名分支，再由团队决定向main合并。本次未自动提交、推送或改写历史。
