# BWTL 安装与更新

适用于 Windows x64、Creo Parametric 10.0.0.0 及 x64 Visual C++ 运行库。AI 需要本机已登录的 Codex。

1. 从 Releases 下载一键安装 EXE，或解压 Windows x64 ZIP 后运行 `install.cmd`。发布页附 SHA-256 校验和。
2. 保留或填写实际的 Creo `parametric.exe` 和 BWTL 安装目录。新安装自动创建独立的 Creo 配置、缓存和工作目录。安装路径建议使用简短的英文路径。
3. 从开始菜单打开「Creo（BWTL）」。Creo 提示加载 BWTL 时按提示确认，再点击 Creo 顶部的 BWTL。重复打开该入口可启动不同编号。已有机器上配置过的 BWTL 专用 Creo 快捷方式仍可使用。
4. 打开插件后先检查「工作目录」和「读取当前模型」。首次测试使用可以丢弃的模型。

需要命令行安装时，在解压目录执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -Silent
```

可加 `-InstallRoot`、`-CreoExecutable` 或 `-CodexPath`。加 `-PlanOnly` 只校验和预览路径。

## 升级与回退

已使用独立配置的安装选择原安装目录即可更新。先保存并正常退出该 Creo 会话，再安装和启动新版。每个会话有独立版本，现有聊天和记忆保存在各自安装目录的 `data`。

0.21.x 的旧托管安装如果仍使用共享配置，保存并退出 Creo 后，从新版 ZIP 运行 `scripts\Enable-BWTLIsolation.ps1 -InstallRoot <原安装目录>`。脚本先备份旧配置并仅迁移已验证属于 BWTL 的注册块。然后用原安装目录运行新安装器。旧版若没有托管状态，可先用旧版卸载入口移除其注册，再按新安装流程操作。

需要回退时，正常退出对应的 Creo 会话，从 BWTL 安装管理选择「回退上一版本」并重新打开该会话。卸载界面可保留聊天和记忆。排查加载问题可先运行安装管理的「导出安装诊断」，并检查该会话自己的 `data\isolation-session.txt`。
