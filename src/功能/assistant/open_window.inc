static int open_window(uiCmdCmdId command, uiCmdValue *value, void *data) {
    HWND owner=GetActiveWindow();DWORD owner_pid=0;
    if(owner)GetWindowThreadProcessId(owner,&owner_pid);
    if(owner_pid!=GetCurrentProcessId())owner=NULL;
    if(!native_ui_open_threaded(owner)){trace("Create native chat window",GetLastError());return PRO_TK_GENERAL_ERROR;}
    native_force_follow=1;
    if(created){native_ui_history(history);return 0;}
    created=1;
    history[0]=0;
    if(conversation[0])append(conversation);
    else native_ui_history(history);
    return 0;
}
