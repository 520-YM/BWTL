# 霸威天龙 BWTL

Creo Parametric 内的 Windows 原生 AI 助手。界面版本 v18；首个仓库版本 **0.18.0-preview.1**。通过 Creo 进程内 Pro/TOOLKIT 执行已实现的操作，不使用浏览器、MCP 或截图键鼠模拟。

**这是预发布版本，并非任意自然语言都能建模的完整 CAD 代理。** 当前支持基本信息读取、模板空模型、简单基础拉伸及部分有校验的尺寸/参数工具。已有实体上继续拉伸、自动装配、真正钣金建模及工程图仍未实现。各项状态见[功能表](docs/功能表.md)。

## 下载与安装

1. 在 [Releases](https://github.com/520-YM/BWTL/releases) 下载 Windows x64 ZIP；若发行流程尚未完成，可在 `dist/` 获取同名包。
2. 解压到固定、可写目录，例如 `D:\BWTL`；不要只复制 DLL，也不要装好后移动目录。
3. 准备 Creo Parametric 10.0.0.0、x64 Microsoft C++ 运行库，以及本机已登录的 Codex 引擎。先阅读包内 `DEPLOYMENT.json` 的许可状态。
4. 双击 `install.cmd`，输入 Creo 启动实际读取的 `config.pro` 完整路径，或按[完整安装说明](docs/INSTALL.zh-CN.md)执行带参数安装。
5. 保存自己的 Creo 工作并重启。首次出现 Creo 插件信任提示时由使用者处理。在图形区依次按 `c`、`d`、`x` 打开“霸威天龙 v18”。
6. 先读取模型，再测试中文聊天，最后在测试空零件里验证建模。未验证成功的路径不得用于替代重要设计工作的人工复核。

安装包不包含 Creo、PTC SDK、公司模板或 Codex 的登录凭据；不同大版本的 Creo 需重新编译并验证。安装器不覆盖其他工具的注册配置，不自动启动/关闭 Creo。

## 数据与历史版本

- `owner-backup/data/`：按仓库所有者要求保留的完整聊天、近期上下文、记忆、待执行状态及功能表。**这个公开仓库中的备份内容可被任何访问者读取。**
- `archive/v18-local-snapshot.tar.xz`：发布时本机助手目录的完整快照，包含旧 DLL、源码备份、资源、日志和旧注册配置。它用于存档，含原机器绝对路径，**不是跨电脑安装包**。
- 默认安装创建空的运行 `data/`，不会自动导入所有者的聊天记忆。迁移本人数据的方法见安装说明。
- `owner-backup/manifest.json` 逐项记录原文件的大小和 SHA-256。历史文件按原样保留，不伪造旧版本的 Git 源码提交。

## 开发与版本管理

源码在 `src/`，PTC SDK 由开发者自行合法安装。设置 `PTK` 后运行 `build.cmd`；SDK 和编译产物中间文件不提交。[版本流程](docs/VERSIONING.md)说明 VERSION、CHANGELOG、校验和、Git 标签及预发布 Release 的对应关系。

源代码发布不等于授予第三方 SDK、模板或产品的授权。应用分发解锁及可能的产品模块要求按 PTC 官方流程处理；当前状态如实记录在 `DEPLOYMENT.json`。

完整历史存档可用 Windows 终端的 `tar -xf v18-local-snapshot.tar.xz` 或支持 XZ 的解压软件展开；普通用户安装仍使用 Windows ZIP 程序包。
