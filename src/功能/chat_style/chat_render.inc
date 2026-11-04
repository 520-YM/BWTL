static int chat_render(HWND history,const wchar_t *source){
    RECT area;HDC dc;int dpi,width;char *rtf;ChatStream stream;EDITSTREAM edit={0};
    GetClientRect(history,&area);dc=GetDC(history);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(history,dc);
    width=MulDiv(area.right-16,1440,dpi?dpi:96);if(width<1800)width=1800;
    rtf=chat_rtf_build(source,width);if(!rtf)return 0;
    stream.text=rtf;stream.length=strlen(rtf);stream.offset=0;
    edit.dwCookie=(DWORD_PTR)&stream;edit.pfnCallback=chat_stream_read;
    SendMessageW(history,WM_SETREDRAW,FALSE,0);
    SendMessageW(history,EM_STREAMIN,SF_RTF,(LPARAM)&edit);
    SendMessageW(history,WM_SETREDRAW,TRUE,0);InvalidateRect(history,NULL,TRUE);free(rtf);
    return !edit.dwError;
}
