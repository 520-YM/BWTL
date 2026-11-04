static int native_ui_create(HWND owner,int show){
    WNDCLASSW cls={0};HINSTANCE module=GetModuleHandleW(NULL);int i;HDC dc;int dpi;
    const wchar_t *labels[3]={L"读取当前模型",L"发送给 AI",L"关闭"};
    int ids[3]={CHAT_READ,CHAT_SEND,CHAT_CLOSE};
    if(native_window){if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);return 1;}
    cls.lpfnWndProc=native_window_proc;cls.hInstance=module;cls.lpszClassName=L"CreoCodexNativeChatV11";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    cls.lpszClassName=L"CreoCodexNativeChatV14";
    native_background=CreateSolidBrush(RGB(224,224,224));cls.hbrBackground=native_background;
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    native_window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"霸威天龙 v21.2",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,880,660,owner,NULL,module,NULL);
    if(!native_window)return 0;
    native_title=CreateWindowW(L"STATIC",L"霸威天龙 v21.2",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_heading=CreateWindowW(L"STATIC",L"原生助手  ·  对话、记忆与建模",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_input_label=CreateWindowW(L"STATIC",L"消息    Enter 发送  /  Shift+Enter 换行",WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_rich_module=LoadLibraryW(L"Msftedit.dll");
    if(!native_rich_module){DestroyWindow(native_window);return 0;}
    native_history=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    native_prompt=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    for(i=0;i<3;i++)native_buttons[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,native_window,(HMENU)(INT_PTR)ids[i],module,NULL);
    if(!native_history||!native_prompt||!native_heading||!native_buttons[0]||!native_buttons[1]||!native_buttons[2]){DestroyWindow(native_window);return 0;}
    SendMessageW(native_history,EM_EXLIMITTEXT,0,150000);
    dc=GetDC(native_window);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(native_window,dc);
    native_font=CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    native_title_font=CreateFontW(-MulDiv(17,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(native_title,WM_SETFONT,(WPARAM)native_title_font,TRUE);
    SendMessageW(native_input_label,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,EM_SETBKGNDCOLOR,0,RGB(232,232,232));
    SendMessageW(native_prompt,EM_SETBKGNDCOLOR,0,RGB(245,245,245));
    SendMessageW(native_heading,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_prompt,WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<3;i++)SendMessageW(native_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_history,EM_SETLIMITTEXT,12000,0);SendMessageW(native_prompt,EM_SETLIMITTEXT,6000,0);
    native_prompt_original=(WNDPROC)SetWindowLongPtrW(native_prompt,GWLP_WNDPROC,(LONG_PTR)native_prompt_proc);
    native_layout();if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);
    return 1;
}
