# 安装、迁移、升级和回退

## 1. 环境准备

- Windows x64；当前编译目标是 Creo Parametric **10.0.0.0**。不能保证其他大版本直接使用。
- 安装 x64 Microsoft Visual C++ 运行库（本项目使用 VS2019 /MD 构建）。
- 安装并正常登录本机 Codex，引擎需可联网。其他使用者使用自己的登录状态；项目不提供账号、令牌或登录文件。
- Creo 实体、钣金和装配默认模板在各自电脑上配置。公司模板不随包分发。
- 查看包内 `DEPLOYMENT.json`。若未完成 PTC 官方分发解锁，运行需要相应 TOOLKIT 开发许可，不能宣称普通 Creo 安装即可运行。官方规则：https://support.ptc.com/help/creo_toolkit/protoolkit_pma/r12/usascii/creo_toolkit/user_guide/Unlocking_a_Creo_Toolkit_Application.html

## 2. 解压程序

从 Releases 或仓库 `dist/` 下载 Windows x64 ZIP，完整解压到固定可写目录，例如 `D:\BWTL`。其中应有 `bin/BWTL.dll`、`resources/text/`、`install.ps1`、`install.cmd`、`VERSION` 和 `DEPLOYMENT.json`。不要使用 `archive/` 内的本机历史快照安装。

## 3. 确定实际使用的 config.pro

使用你自己电脑上 Creo 启动确实会读取的 `config.pro`。公司的启动器可能使用统一标准目录；这时向管理员确认实际路径。不能随便在一个目录新建 config.pro 就认为 Creo 会读到它。

如果原来已安装旧版助手，先备份并停用旧助手的单条 `protkdat` 配置，避免相同命令注册两次；不要删除 WJT 等其他工具条目。

## 4. 注册插件

最简单：双击 `install.cmd`，输入上一节的 config.pro 完整路径。

也可在 PowerShell 中执行（替换为本机路径）：

```powershell
cd D:\BWTL
.\install.ps1 -ConfigPath 'C:\YourCreoStartup\config.pro'
```

想先看计划而不修改文件：

```powershell
.\install.ps1 -ConfigPath 'C:\YourCreoStartup\config.pro' -PlanOnly
```

安装器先备份现有配置，再生成本机绝对路径的 protk.dat/settings.ini，并在 config.pro 写入 `! BWTL BEGIN` 到 `! BWTL END` 的独立注册块。不会替换整个公司配置，不会添加公司的模板路径。

脚本执行策略阻止运行时，可使用包内 install.cmd；它仅为该次脚本进程设置执行策略，不永久修改系统策略。

## 5. Codex 路径配置

默认查找 PATH 中的 codex.exe 和 `%LOCALAPPDATA%\OpenAI\Codex\bin\*\codex.exe`。若安装位置不同，指定真实存在的原生 codex.exe：

```powershell
.\install.ps1 -ConfigPath 'C:\YourCreoStartup\config.pro' -CodexPath 'C:\YourCodexInstall\codex.exe'
```

也可编辑 settings.ini 的 `[Runtime]` / `CodexPath`。此版本不把 codex.cmd 当作原生可执行文件运行。更改后重新启动 Creo。

## 6. 模板配置

在 Creo 中为本机实际模板设置以下选项，路径由使用者确定：

```text
template_solidpart <实体空零件模板路径>
template_sheetmetalpart <钣金空零件模板路径>
template_designasm <空装配模板路径>
```

模板类型必须正确，空模板不应已经含有实体几何或装配组件。基础拉伸要求模板保留三基准面和坐标系。不要把别人的模板绝对路径原样照抄。

## 7. 打开及验收

1. 自行保存正在编辑的模型，再重启 Creo。
2. 如 Creo 弹出插件信任确认，由使用者按当地部署规则处理；安装器不代点。
3. 在图形区依次按 c、d、x，打开“霸威天龙 v18”。也可在 Creo 自定义界面中添加已注册的原生助手命令；不导入整个原电脑的功能区文件。
4. 打开一个测试零件，点击“读取当前模型”：应显示简短基本信息。
5. 发送一条中文聊天，核对 AI 回复及不同类型消息底色。
6. 用公司默认模板创建测试实体空零件，再输入“在当前零件拉伸一个边长20毫米的正方体，不新建零件”。检查模型名、基准和实际几何。
7. 尺寸、参数和抑制/恢复工具首次写入前会运行核心自检；失败时看数据目录的 part-selfcheck.log，不得把失败当作成功。

本次公开包的跨电脑完整建模流程尚未在另一台机器上验证。现有模型上继续加料、自动加入装配、钣金壁和工程图仍不支持。

## 8. 本人的数据迁移

新安装默认数据目录为解压目录下的 data；不自动载入仓库所有者的私人数据。

仓库所有者本人需要恢复备份时，将仓库里的 `owner-backup` 放到安装目录，并选择一个空数据目录：

```powershell
.\install.ps1 -ConfigPath 'C:\YourCreoStartup\config.pro' -DataDirectory 'D:\BWTL-MyData' -ImportOwnerData
```

安装器拒绝覆盖已有同名数据文件。也可以退出 Creo 后手动备份和复制自己的 data 文件。pending-plan.txt 仅作状态存档，新会话不重放旧建模指令。聊天请求及最后一次回复的临时文件仍在 `%TEMP%\CreoCodex-进程号\`。

## 9. 升级及回退

升级前保存模型并退出 Creo，备份 data、settings.ini 和实际 config.pro。把新包解压到新目录，使用同一独立 DataDirectory，重新运行安装器更新 BWTL 管理块。不要覆盖正在加载的 DLL。

回退时退出 Creo，恢复旧包及对应 config.pro 备份；不要从旧发行包覆盖当前聊天数据。

## 10. 卸载

```powershell
.\uninstall.ps1 -ConfigPath 'C:\YourCreoStartup\config.pro'
```

脚本仅移除 BWTL 自己的注册块，保留程序、数据及其他工具配置；下次启动 Creo 生效。若原先为旧版手工添加了未标记的 protkdat，需自行停用对应的那一行。
