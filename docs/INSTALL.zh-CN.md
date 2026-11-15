# BWTL 安装与更新

本版适用于 Windows x64、Creo Parametric 10.0.0.0、x64 Visual C++ 运行库和可写入的本地固定磁盘。AI 使用本机已登录的 Codex。

1. 从 Releases 下载 EXE 或 Windows x64 ZIP，并对照随附 SHA-256 校验。使用 ZIP 时解压后运行 install.cmd。
2. 确认实际的 Creo parametric.exe。在「数据盘」下拉框选择电脑上可用的盘符，再填写或浏览选择文件夹。未设置过安装目录时，默认显示当前用户 AppData 目录下的 BWTL。新安装会在所选盘建立独立配置、工作目录、缓存、日志和临时目录。
3. 从开始菜单打开「Creo（BWTL）」。已接入 BWTL 的原有 Creo 快捷方式可继续使用。Creo 加载后点击顶部 BWTL，使用「工作目录」检查当前会话的路径是否位于所选盘。
4. 在独立测试零件中先读取模型，再使用「截图改图」核对整窗、标注和实际读回。

可通过命令行安装。加 -PlanOnly 可校验安装包和预览安装路径。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1 -Silent -InstallRoot D:\BWTL
```

可通过 -CreoExecutable 和 -CodexPath 指定本机程序。命令示例以 D 盘为例，可换成任意可用的本地固定盘。程序、聊天、记忆、工作区、缓存、日志和备份使用所选盘。数据盘不可用时，启动会给出提示。

## 升级与回退

升级前保存手头模型并正常退出对应的 Creo 会话。选择原来的 BWTL 独立安装目录。各会话的数据保存在各自的 data。选另一盘上的新目录会建立另一套安装。

需要恢复时，从安装管理选择「回退上一版本」，再打开对应 Creo 会话。首次全新安装需要先有可用的上一版才能通过安装管理回退。排查加载可检查相应会话的 data\isolation-session.txt。