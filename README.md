# BWTL

BWTL 是 Creo Parametric 10.0.0.0 的 Windows x64 原生插件。当前公开版为 **0.23.0-preview.1**，以本机已验证的多会话版本为基础。

## 下载与安装

1. 打开 [GitHub Releases](https://github.com/520-YM/BWTL/releases)，下载一键安装 EXE 或 Windows x64 ZIP。ZIP 解压后运行 `install.cmd`。
2. 安装时选择 Creo 的 `parametric.exe`。默认创建独立配置。安装路径建议使用简短的英文路径。
3. 从开始菜单打开 **Creo（BWTL）**，在 Creo 顶部选择 BWTL。按 Creo 提示确认本机插件的加载。
4. AI 使用你在本机登录的 Codex。模型读取和原生菜单可以先在测试零件上验证。

已有独立 BWTL 安装可以用原目录升级。使用共享 Creo 配置的旧版要先迁移。安装、验证和回退方法见[安装说明](docs/INSTALL.zh-CN.md)。已经接入 BWTL 的原有 Creo 快捷方式可继续使用。

## 功能

- 多个独立 Creo 会话可以并行运行，各自保存聊天、记忆、工作目录和插件版本。
- 聊天支持智能协作、只分析和功能开发三种模式，支持图片附件。功能开发可在独立源码副本内进行编译和回归验证。
- 支持读取模型、在工作目录打开零件、创建基础实体、草绘、拉伸、旋转、钻孔、圆角和倒角。能力边界和用法见[功能列表](docs/BWTL%20功能列表.txt)。

新版安装包提供源码、运行资源、测试和功能指南。新安装从空白聊天开始，升级保留已有本机聊天及记忆。

## 开发

源码在 `src/`，冻结兼容清单在 `compatibility/`。本机安装 PTC SDK 后，设置 `PTK` 并运行 `build.cmd`。查看[发行与校验流程](docs/VERSIONING.md)和[本次发行说明](docs/releases/0.23.0-preview.1.md)。
