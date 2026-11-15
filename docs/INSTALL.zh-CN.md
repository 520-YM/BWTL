# BWTL 安装与更新

本版适用于 Windows x64、Creo Parametric 10.0.0.0、x64 Visual C++ 运行库和本地 E 盘。AI 使用本机已登录的 Codex。

1. 从 Releases 下载 EXE 或 Windows x64 ZIP，并对照随附 SHA-256 校验。使用 ZIP 时解压到 E 盘并运行 install.cmd。
2. 确认实际的 Creo parametric.exe 和 E 盘安装目录。首次安装默认使用 E:\BWTL。已设置 BWTL_INSTALL_ROOT 的本机会显示已有目录。新安装创建独立配置、工作目录、缓存、日志与临时目录。
3. 从开始菜单打开「Creo（BWTL）」。已接入 BWTL 的原有 Creo 快捷方式可继续使用。Creo 加载后点击顶部 BWTL，使用「工作目录」检查当前会话的 E 盘路径。
4. 在独立测试零件中先读取模型，再使用「截图改图」核对整窗、标注和实际读回。

可通过命令行安装。加 -PlanOnly 可校验安装包和预览安装路径。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -Silent -InstallRoot E:\BWTL
```

可通过 -CreoExecutable 和 -CodexPath 指定本机程序。E 盘不可用时安装和启动会停止。共享配置的旧用户应先按 [E 盘迁移说明](E盘存储.md) 处理各自安装目录和隔离配置。

## 升级与回退

升级前保存手头模型并正常退出对应的 Creo 会话。选择原 E 盘上的 BWTL 独立安装目录。先有会话的数据保存在各自的 data。

需要恢复时，从安装管理选择「回退上一版本」，再打开对应 Creo 会话。首次全新安装需要先有可用的上一版才能通过安装管理回退。排查加载可检查相应会话的 data\isolation-session.txt。