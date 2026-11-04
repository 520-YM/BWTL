static int open_window(uiCmdCmdId command, uiCmdValue *value, void *data) {
    HWND owner=GetActiveWindow(); DWORD owner_pid=0;
    if(owner)GetWindowThreadProcessId(owner,&owner_pid);
    if(owner_pid!=GetCurrentProcessId())owner=NULL;
    if(!native_ui_open_threaded(owner)){trace("Create native chat window",GetLastError());return PRO_TK_GENERAL_ERROR;}
    native_force_follow=1;
    if(created){native_ui_history(history);return 0;}
    created=1;history[0]=0;
        append(L"AI 对话版：在下方输入框输入文字，按回车或点击“发送给 AI”。\n使用本机 Codex 登录，后台请求不会操作鼠标键盘。\n支持对话、模板新建、零件特征/尺寸/参数/几何读取，以及经过校验的现有零件操作。关闭窗口会取消等待。\n对话、当前零件名称及API读取的特征、尺寸、参数、几何摘要会发送给 AI；请求文件与日志保存在 临时目录 CreoCodex-进程号 文件夹。");
        append(L"聊天记录会自动保存，重启后恢复最近对话。长期记忆命令：记住：内容 / 查看记忆 / 忘记：原内容。\n输入“新对话”清空近期上下文但保留记忆；“查看历史”显示历史位置和近期内容。已保存记忆和近期对话会随请求发送给 AI。");
        append(L"自动建模：直接说明板/长方体/圆柱的毫米尺寸，可带一个中心通孔。完整指令会直接按 Creo 当前默认实体模板创建新零件，保留模板坐标系、基准面和参数；不需二次点击。尺寸不全会询问，不自动保存，复杂建模和二维工程图尚未接入。");
        if (!store_ready) append(L"注意：本地记录目录不可用，持久化功能未启用。");
        if (conversation[0]) { append(L"——已恢复最近聊天——"); append(conversation); }
        append(L"——以上为历史记录。当前为 v21.2：自带Creo菜单，支持基准与关联草绘，以及草绘编辑、关联投影、标准拉伸加料、定深/贯穿切除和旋转加料。本批新增操作限独立普通单实体零件，请提供明确参照、尺寸与方向。——");

    return 0;
}
