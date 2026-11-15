# BWTL 多会话

每双击一次桌面原有的 **Creo Parametric** 图标，就打开一个独立 Creo 进程。启动器选择最小的空闲编号，连点时也为正在启动的窗口保留编号。打开 Creo 顶部的 BWTL 后，聊天窗口标题会显示「会话 1」「会话 2」等编号。已有 Creo 进程保留它加载时的版本。

| 会话 | BWTL 安装、聊天和记忆 | Creo 配置、默认工作目录和缓存 |
| --- | --- | --- |
| 1 | `%LOCALAPPDATA%\BWTL` | `%LOCALAPPDATA%\BWTL-Profile` |
| 2 起 | `%LOCALAPPDATA%\BWTL-Instances\编号` | `%LOCALAPPDATA%\BWTL-Instances\profiles\编号` |

新编号从当前主版本安装独立副本，聊天和记忆从空白开始。退出后，该编号的记录保存在原目录。以后再次打开这个编号即可续用。窗口内发起的功能开发、验证和升级只使用本会话的源码及安装路径。主会话继续用原来的源码目录。每套会话的默认工作目录独立。主动把多个会话切换到同一目录后，该目录里的模型文件会共用。

PTC 要求并行 Creo 会话使用不同的 WFROOT 缓存。各会话另外使用自己的注册、临时目录、工作目录和日志，保留 Creo 原有的加载信任和许可证检查。[PTC 会话与 WFROOT 文档](https://support.ptc.com/help/creo/creo_pma/r13/usascii/fundamentals/fundamentals/retrieving_model_from_last_session.html)

验证时从原图标连续启动，在各窗口打开 BWTL 核对会话编号，并分别查看工作目录。各自安装目录下的 `data\isolation-session.txt` 记录进程号、DLL 和配置路径。

需要回退时，先正常退出要回退的 Creo 会话。会话 1 使用现有安装管理的「回退上一版本」，并从更新备份恢复主安装目录的 `Start-BWTL.ps1`。其他编号的目录和历史仍保存在 `BWTL-Instances`。已经单独升级过的编号可以在自己的安装目录运行安装管理回退。
