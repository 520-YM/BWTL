# BWTL

BWTL 是 Creo Parametric 10.0.0.0 的 Windows x64 原生插件。当前预发行版为 **0.23.0-preview.7**，安装时可选择本地数据盘。

- **截图改图**支持整窗截图、圈选、箭头、框选、文字贴注和最多 8 处标注。可修改独立孔径或受支持的矩形贯穿开口，结果由 Creo 再生并读回。
- **仿照模型**从空白件按源特征树重建可编辑原生特征，支持批量预检、建模方法学习和标准件入库。100 项清单中有 99 项具备至少一种验收路径，具体几何、参照条件及待验证范围见说明。 [仿照模型](docs/仿照模型.md)
- **插件功能**提供零件、装配、钣金、制造和交付工具。 [功能列表](docs/BWTL%20功能列表.txt)
- 多个 BWTL Creo 会话各自使用独立的版本、工作目录、聊天和记忆。

## 安装与更新

在 [GitHub Releases](https://github.com/520-YM/BWTL/releases) 下载 EXE 或 Windows x64 ZIP。需要 Creo 10.0.0.0。AI 使用你在本机已登录的 Codex。使用 ZIP 时解压后运行 install.cmd。在安装界面选择数据盘及文件夹。已有安装选择原目录升级。 [安装、校验与回退](docs/INSTALL.zh-CN.md)

C 盘迁移核对见 [E 盘存储与迁移说明](docs/E盘存储.md)。完整变更见 [发行说明](docs/releases/0.23.0-preview.7.md) 和 [更新记录](CHANGELOG.md)。

## 开发

源码在 src，功能分组与文件对应关系在 docs/feature-files.json。编译、回归和发行包生成方法见 [版本管理](docs/VERSIONING.md)。

装配学习及通过验收的原特征重建会自动保存本地记忆。安装包附带通用装配与钣金案例，在「仿照模型 → 查看学习记忆」核对存档。
