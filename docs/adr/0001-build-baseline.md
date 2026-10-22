# ADR 0001：锁定可重建的 Qt 工具链

- 状态：接受。
- 决策：锁定 Qt 6.10.2 EXACT、MinGW-w64 GCC 13.1 x64、CMake 3.24+、yaml-cpp 0.8.0。后续官方 SDK 获取方式见 ADR 0016。
- 路径由环境变量和项目相对目录指定，不绑定某台开发工作站。
- MSVC Ninja 入口必须匹配 MSVC Qt kit；不能链接 MinGW Qt。MSVC 与 Qt5 移植未验证，不作为已支持能力。
- 目标为 Windows 10/11，实际系统验收与构建验证分别记录。
- 依据：[Qt 6.10 官方支持平台](https://doc.qt.io/qt-6.10/supported-platforms.html)。
