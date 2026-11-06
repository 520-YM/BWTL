static int open_window(uiCmdCmdId command, uiCmdValue *value, void *data) {
    HWND owner=GetActiveWindow(); DWORD owner_pid=0;
    if(owner)GetWindowThreadProcessId(owner,&owner_pid);
    if(owner_pid!=GetCurrentProcessId())owner=NULL;
    if(!native_ui_open_threaded(owner)){trace("Create native chat window",GetLastError());return PRO_TK_GENERAL_ERROR;}
    native_force_follow=1;
    if(created){native_ui_history(history);if(development_process)native_schedule_poll();return 0;}
    created=1;history[0]=0;
        append(L"BWTL智能协作：可以先讨论设计需求、比较方案和分析图片。智能协作模式会自动读取参照、执行已明确的一步、核对真实结果并继续；只分析模式不修改模型。可随时点停止。默认不自动保存模型。");
        append(L"图片可粘贴或拖入。也可输入“添加图片”选择文件，输入“清除图片”清空当前附件。图片随消息发送给 AI。");
        append(L"切换到功能开发后可提出插件修改需求。需要安装通过检查的升级时，在输入框发送“应用已验升级”。");
        append(L"聊天与记忆会自动保留。可输入“执行明细”查看原生回执。长期记忆命令包括“记住：内容”“查看记忆”和“忘记：原内容”。");
        if (!store_ready) append(L"注意：本地记录目录不可用，持久化功能未启用。");
        if (conversation[0]) { append(L"——已恢复最近聊天——"); append(conversation); }
        append(L"——以上为历史记录。当前为 BWTL v23 智能协作版：支持图片分析、连续任务和窗口内功能开发；原有建模工具保留，执行以本次原生回执为准。——");

    return 0;
}
