# E 盘存储与本机迁移

2026 年 11 月 12 日起，本机 BWTL 数据根目录为 E:\BWTL-Data。原有文件名和目录名已保留。已有桌面 Creo 入口按本机迁移配置启动 BWTL。从 0.23.0-preview.4 起，新用户可在安装时选择任意可用的本地固定数据盘。本页 C 到 E 的记录对应原机器的迁移。

## C 盘到 E 盘的目录对照

| 迁移内容 | C 盘原位置，相对于 Windows 用户目录 | E 盘位置，相对于 E:\BWTL-Data | 迁移清单条目 |
| --- | --- | --- | ---: |
| BWTL 主安装、历史版本、备份、聊天、记忆、公司资料 | AppData\Local\BWTL | AppData\Local\BWTL | 1 |
| 各独立实例及其配置、数据 | AppData\Local\BWTL-Instances | AppData\Local\BWTL-Instances | 1 |
| 主 Creo 专用配置、缓存、日志、工作区 | AppData\Local\BWTL-Profile | AppData\Local\BWTL-Profile | 1 |
| 源码、Git 工作树及历史开发档案 | Documents 下的 BWTL 相关目录和文件 | Documents 下对应的原名目录和文件 | 67 |
| 测试证据和可视化档案 | .codex\visualizations 下的迁移条目 | .codex\visualizations 下对应的原名目录 | 44 |
| BWTL 的临时验收档案 | AppData\Local\Temp 下的迁移条目 | AppData\Local\Temp 下对应的原名目录 | 139 |
| 桌面安装包与使用说明 | Desktop | Desktop 下对应原名文件 | 2 |

合计 255 个迁移条目。一个目录条目可以包含多个文件。本机完整逐条路径见 E:\BWTL-Data\migration-inventory.json，校验记录见该目录下的 migration-verified-*.json。迁移前快捷方式的备份位于 E:\BWTL-Data\Migration-Backup\Shortcuts。

GitHub 上的本页提供目录对照。迁移中的本机聊天、记忆、公司标准原件、测试模型、配置备份及临时文件仍可在上表对应目录查阅。

## 使用、验证与回退

从 BWTL 的「工作目录」选择当前所选数据盘上的项目目录。可读取其他磁盘上的源模型，插件生成文件使用所选数据盘上的工作目录。工作区、临时目录、缓存和日志与旧 Codex–Creo 桥接隔离。

迁移时在独立 Creo 10 验证了 E 盘实际加载，以及 E 盘创建和保存。当前预发行版安装步骤见 [安装与更新](INSTALL.zh-CN.md)。正常退出需要回退的 Creo 会话后，可用其 BWTL 安装管理恢复上一版。迁移原始路径和验证记录可用于核对迁移范围。