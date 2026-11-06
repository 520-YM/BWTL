# 版本管理

- `VERSION`、`CHANGELOG.md`、`DEPLOYMENT.json` 与 `docs/releases/<版本>.md` 随版本更新。发行后的包不覆盖。
- 在安装了 Creo 10、PTC SDK 和 Visual Studio C++ 工具的 Windows 主机上编译。按 PTC 流程解锁 DLL，把已验证的程序放入 `bin/BWTL.dll` 并登记 SHA-256。
- 运行 `setup/Build-Setup.ps1 -WorkDirectory <源码目录之外的临时目录>`，生成 Windows x64 ZIP、安装 EXE 及版本校验和。公共包仅包含程序、公开源码、资源、文档、测试和功能指南。
- 检查包清单、解压、独立配置和升级。把新版产物及发行说明提交到 `main` 后，GitHub Actions 核对校验和，再以 `v<VERSION>` 标签发布预发行版。
- 按各会话的安装管理流程回退上一版本。历史发行版及其标签保留。
