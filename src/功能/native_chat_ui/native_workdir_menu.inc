static void native_workdir_menu(HWND button){
    HMENU menu=CreatePopupMenu();
    RECT anchor;
    UINT action=0;
    if(!menu)return;
    if(GetWindowRect(button,&anchor)&&
       AppendMenuW(menu,MF_STRING,WORKDIR_VIEW,L"查看当前工作目录")&&
       AppendMenuW(menu,MF_STRING,WORKDIR_OPEN,L"打开当前工作目录")&&
       AppendMenuW(menu,MF_STRING,WORKDIR_COPY,L"复制工作目录路径")&&
       AppendMenuW(menu,MF_SEPARATOR,0,NULL)&&
       AppendMenuW(menu,MF_STRING,WORKDIR_PICK,L"切换工作目录…")){
        action=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY,anchor.left,anchor.bottom,native_window,NULL);
    }
    DestroyMenu(menu);
    if(action&&!native_busy)native_workdir_action(action);
}
