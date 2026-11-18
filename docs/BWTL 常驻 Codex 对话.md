# BWTL 常驻 Codex 对话

Creo 里的 BWTL 首次需要 AI 时启动自己的 Codex App Server，会话期间复用同一个服务和对话。登录由本机 Codex 管理。每个 Creo 实例使用各自 E 盘 BWTL 数据目录，关闭 Creo 时释放服务。重开 Creo 后，服务建立新的空白聊天会话。每个进程的聊天和附件分别保存在 E 盘。长期记忆和学习方法按保存的规则继续使用。输入「新对话」可主动开启新会话。

可以继续追问刚才的模型，粘贴或拖入图片，也可以在消息里输入要读的 E 盘文本文件完整路径。文本读取支持 UTF-8 和 UTF-16 LE 编码的 TXT、MD、CSV、TSV、JSON、LOG。单次最多读取 128 KB 且受回执长度限制。

Codex 通过 BWTL 原生工具读取模型、执行操作并取得 Creo 回执。同一轮内可根据回执检查模型、调整参照并尝试完成后续步骤。模型操作仍由 Creo 中的 BWTL 校验、绑定并执行。点击「停止」中断本轮，已经完成的 Creo 操作保留。聊天窗仍以一条结果回复为主。

维护时可以在 BWTL 数据目录的 codex-session-* 下核对 connection.txt、thread.txt 和连接日志。需要回退时在 BWTL 管理工具选择 0.23.0-local.newmodel.1，再正常重开 Creo。

协议依据见 [OpenAI Docs | Codex App Server](https://developers.openai.com/codex/app-server)。0.164.0 本地协议使用 stdio、会话、动态工具以及图像输入。常驻服务关闭额外 MCP 工具加载。
