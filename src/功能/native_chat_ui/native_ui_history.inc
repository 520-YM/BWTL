static int native_ui_history(const wchar_t *text) {
    SCROLLINFO info={sizeof(info),SIF_ALL};
    int first,follow; LRESULT length;
    size_t count;
    if(!IsWindow(native_history))return 0;
    if(native_ui_thread_id && GetCurrentThreadId()!=native_ui_thread_id)
        return (int)SendMessageW(native_window,CHAT_HISTORY_TEXT,0,(LPARAM)text);
    first=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);
    GetScrollInfo(native_history,SB_VERT,&info);
    follow=native_force_follow || info.nMax<=0 || info.nPos >= info.nMax-(int)info.nPage-1;
    if(text!=native_render_source){
        wchar_t *copy;count=wcslen(text);copy=(wchar_t *)calloc(count+1,sizeof(wchar_t));if(!copy)return 0;
        wcscpy_s(copy,count+1,text);free(native_render_source);native_render_source=copy;
    }
    if(!chat_render(native_history,native_render_source))return 0;
    if(follow){
        length=GetWindowTextLengthW(native_history);
        SendMessageW(native_history,EM_SETSEL,length,length);
        SendMessageW(native_history,EM_SCROLLCARET,0,0);
        SendMessageW(native_history,WM_VSCROLL,SB_BOTTOM,0);
    }else{
        int now=(int)SendMessageW(native_history,EM_GETFIRSTVISIBLELINE,0,0);
        SendMessageW(native_history,EM_LINESCROLL,0,first-now);
    }
    native_force_follow=0;
    return 1;
}
