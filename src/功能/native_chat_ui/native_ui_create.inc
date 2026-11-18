static int native_ui_create(HWND owner,int show){
    WNDCLASSW cls={0};HINSTANCE module=GetModuleHandleW(NULL);int i;HDC dc;
    const wchar_t *labels[3]={L"读取当前模型",L"发送给 AI",L"关闭"};
    int ids[3]={CHAT_READ,CHAT_SEND,CHAT_CLOSE};
    const wchar_t *top_labels[5]={L"插件功能",L"标准文件",L"工作目录",L"仿照模型",L"截图改图"};
    wchar_t slot[12], title[32]=L"BWTL";
    DWORD slot_length=GetEnvironmentVariableW(L"BWTL_INSTANCE_NUMBER",slot,_countof(slot));
    if(slot_length && slot_length<_countof(slot) && wcsspn(slot,L"0123456789")==slot_length)
        swprintf_s(title,_countof(title),L"BWTL · 会话 %ls",slot);
    if(native_window){if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);return 1;}
    cls.lpfnWndProc=native_window_proc;cls.hInstance=module;cls.lpszClassName=L"CreoCodexNativeChatV11";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    cls.lpszClassName=L"CreoCodexNativeChatV14";
    native_background=CreateSolidBrush(CHAT_CANVAS_COLOR);cls.hbrBackground=native_background;
    native_header_brush=CreateSolidBrush(CHAT_HEADER_COLOR);
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    native_window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,title,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,980,820,owner,NULL,module,NULL);
    if(!native_window)return 0;
    native_theme_caption(native_window);
    native_title=CreateWindowW(L"STATIC",title,WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_elapsed_label=CreateWindowW(L"STATIC",L"等待指令",WS_CHILD|WS_VISIBLE|SS_RIGHT,0,0,0,0,native_window,NULL,module,NULL);
    for(i=0;i<5;i++){
        DWORD style=WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_TABSTOP;
        native_top_buttons[i]=CreateWindowW(L"BUTTON",top_labels[i],style,
            0,0,0,0,native_window,(HMENU)(INT_PTR)(CHAT_FEATURES+i),module,NULL);
        if(!native_top_buttons[i]){DestroyWindow(native_window);return 0;}
    }
    native_stop_button=CreateWindowW(L"BUTTON",L"停止",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_STOP,module,NULL);
    native_preview=CreateWindowW(L"STATIC",L"",WS_CHILD|SS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_PREVIEW,module,NULL);
    native_attach_button=CreateWindowW(L"BUTTON",L"＋ 附件",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_ATTACH,module,NULL);
    native_clear_button=CreateWindowW(L"BUTTON",L"清空附件",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_CLEAR_IMAGES,module,NULL);
    DragAcceptFiles(native_window,TRUE);
    native_rich_module=LoadLibraryW(L"Msftedit.dll");
    if(!native_rich_module){DestroyWindow(native_window);return 0;}
    native_history=CreateWindowExW(0,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    native_prompt=CreateWindowExW(0,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    DragAcceptFiles(native_prompt,TRUE);
    for(i=0;i<3;i++)native_buttons[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,native_window,(HMENU)(INT_PTR)ids[i],module,NULL);
    if(!native_attach_button||!native_clear_button||!native_history||!native_prompt||!native_title||!native_elapsed_label||!native_preview||!native_stop_button||
        !native_buttons[0]||!native_buttons[1]||!native_buttons[2]){DestroyWindow(native_window);return 0;}
    SendMessageW(native_history,EM_EXLIMITTEXT,0,150000);
    dc=GetDC(native_window);
    if(dc){native_dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(native_window,dc);}
    native_font=CreateFontW(-MulDiv(10,native_dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    native_title_font=CreateFontW(-MulDiv(17,native_dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    SendMessageW(native_title,WM_SETFONT,(WPARAM)native_title_font,TRUE);
    SendMessageW(native_elapsed_label,WM_SETFONT,(WPARAM)native_font,TRUE);
    native_request_timer_render();
    SendMessageW(native_history,EM_SETBKGNDCOLOR,0,CHAT_CANVAS_COLOR);
    SendMessageW(native_prompt,EM_SETBKGNDCOLOR,0,CHAT_INPUT_COLOR);
    SendMessageW(native_history,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_prompt,WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<3;i++)SendMessageW(native_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<5;i++)SendMessageW(native_top_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_stop_button,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_attach_button,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_clear_button,WM_SETFONT,(WPARAM)native_font,TRUE);
    EnableWindow(native_stop_button,FALSE);
    SendMessageW(native_history,EM_SETLIMITTEXT,12000,0);SendMessageW(native_prompt,EM_SETLIMITTEXT,6000,0);
    native_prompt_original=(WNDPROC)SetWindowLongPtrW(native_prompt,GWLP_WNDPROC,(LONG_PTR)native_prompt_proc);
    native_layout();if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);
    return 1;
}
