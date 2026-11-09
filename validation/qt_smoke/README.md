# Qt集成检查

在初始化MSVC的开发终端运行（当前项目根目录）：

```text
cmake -S validation/qt_smoke -B build-qt -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/QT/6.5.3/msvc2019_64
cmake --build build-qt
set QT_QPA_PLATFORM=offscreen
set PATH=D:\QT\6.5.3\msvc2019_64\bin;%PATH%
build-qt\qt_install_check.exe
```

期望输出：Qt6.5.3: Widgets+SQLite+real analysis integration OK（空格以程序实际输出为准）。不打开可见窗口，使用离屏插件。测试SQL只证明驱动可用，不代表业务持久化已实现。
