static int open_window(uiCmdCmdId command, uiCmdValue *value, void *data) {
    HWND owner=GetActiveWindow(); DWORD owner_pid=0;
    if(owner)GetWindowThreadProcessId(owner,&owner_pid);
    if(owner_pid!=GetCurrentProcessId())owner=NULL;
    if(!native_ui_open_threaded(owner)){trace("Create native chat window",GetLastError());return PRO_TK_GENERAL_ERROR;}
    native_force_follow=1;
    if(created){native_ui_history(history);return 0;}
    created=1;history[0]=0;
        append(L"BWTL 可讨论设计、分析图片并执行 Creo 建模。直接说明要做的操作，BWTL 会核对参照、执行并汇报结果。只想看方案时可说「只分析」。建模默认由你保存，可随时点停止。");
        append(L"图片可粘贴或拖入。也可输入“添加图片”选择文件，输入“清除图片”清空当前附件。图片随消息发送给 AI。");
        append(L"聊天与记忆会自动保留。可输入“执行明细”查看原生回执。长期记忆命令包括“记住：内容”“查看记忆”和“忘记：原内容”。");
        if (!store_ready) append(L"注意：本地记录目录不可用，持久化功能未启用。");
        if (conversation[0]) { append(L"——已恢复最近聊天——"); append(conversation); }
        append(L"——以上为历史记录。当前任务以本次原生回执为准。——");

    return 0;
}
