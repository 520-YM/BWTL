static LRESULT CALLBACK native_window_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    switch(message){
    case CHAT_HISTORY_TEXT:return native_ui_history((const wchar_t *)l);
    case CHAT_DESTROY:DestroyWindow(window);PostQuitMessage(0);return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w,TRANSPARENT);SetTextColor((HDC)w,RGB(43,64,76));return (LRESULT)native_background;
    case WM_DRAWITEM:{
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)l;RECT r=d->rcItem;wchar_t label[40];
        COLORREF color=d->CtlID==CHAT_SEND?RGB(18,120,104):RGB(225,234,239);
        HBRUSH brush;HPEN pen;HGDIOBJ oldbrush,oldpen,oldfont;
        if(d->itemState&ODS_SELECTED)color=d->CtlID==CHAT_SEND?RGB(13,96,83):RGB(208,221,229);
        brush=CreateSolidBrush(color);pen=CreatePen(PS_SOLID,1,color);
        oldbrush=SelectObject(d->hDC,brush);oldpen=SelectObject(d->hDC,pen);
        RoundRect(d->hDC,r.left,r.top,r.right,r.bottom,10,10);
        oldfont=SelectObject(d->hDC,native_font);SetBkMode(d->hDC,TRANSPARENT);
        SetTextColor(d->hDC,d->CtlID==CHAT_SEND?RGB(255,255,255):RGB(36,57,69));
        GetWindowTextW(d->hwndItem,label,40);DrawTextW(d->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(d->hDC,oldfont);SelectObject(d->hDC,oldbrush);SelectObject(d->hDC,oldpen);DeleteObject(brush);DeleteObject(pen);return TRUE;
    }
    case WM_SIZE:native_layout();return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO *)l)->ptMinTrackSize.x=460;((MINMAXINFO *)l)->ptMinTrackSize.y=400;return 0;
    case WM_COMMAND:
        if(LOWORD(w)==CHAT_SEND){native_force_follow=1;native_send_action();return 0;}
        if(LOWORD(w)==CHAT_READ){native_force_follow=1;native_read_action();return 0;}
        if(LOWORD(w)==CHAT_CLOSE){native_close_action();return 0;}
        break;
    case WM_TIMER:if(w==CHAT_POLL){KillTimer(window,CHAT_POLL);native_poll_action();return 0;}break;
    case WM_CLOSE:native_close_action();return 0;
    case WM_DESTROY:
        free(native_render_source);native_render_source=NULL;
        KillTimer(window,CHAT_POLL);native_window=native_history=native_prompt=NULL;
        if(native_font){DeleteObject(native_font);native_font=NULL;}return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
