# 第一周 M0：陈艺夫个人接口与 Mock

本包从已校验的修正版提取，使用C++17。只提交分析/数据管理接口、公共类型提案及Mock，不包含第二周真实分析引擎。

## 对应项目任务

- IAnalysisService、IDataManager两个接口草案。
- core/types.h：分析输入、会话、指标、配置等公共类型建议。
- MockAnalysisService、MockDataManager和演示。
- 17项Mock契约检查。

## 验证依据

本包接口、Mock、示例和测试源码与2026-11-09的Windows复核版一致，已在MSVC19.44环境通过17项Mock检查及模拟演示。本包另提供只包含第一周目标的CMakeLists.txt、week1.pro。

## 未完成或待共同确认

公共类型需要四人确认，尚未冻结；Qt主窗口由UI负责人提供，不在本个人包内。PoseResult、PoseConfig、会话ID和角度定义需与姿态/相机模块确认。孙同学Python Protocol与C++间的通信方式未确定。

Mock分析固定返回8分，并标记simulated=true，不是真实评分。会话和配置只在内存，重启丢失；CSV与报告返回NotImplemented。此包不能冒充第二周离线算法完成。

## 构建

初始化MSVC的开发终端中：

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

qmake可打开week1.pro；请使用英文路径，当前中文路径存在qmake/NMake编码问题。

## 上传

从姓名分支创建chenyifu/week01-m0。复制本包内部文件到本地仓库根目录；已有团队README保留。不要把ZIP、本包外层目录、work或outputs整体提交。

提交说明：feat: add week1 analysis interfaces and mocks。

通过PR合入姓名分支后，从更新后的姓名分支创建chenyifu/week02-analysis，再上传第二周Windows复核包。第二周包会扩展同名CMake文件并新增真实分析及测试，保留第一周内容。
