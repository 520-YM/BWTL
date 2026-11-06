static int native_ui_create(HWND owner,int show){
    WNDCLASSW cls={0};HINSTANCE module=GetModuleHandleW(NULL);int i;HDC dc;int dpi;
    const wchar_t *labels[3]={L"读取当前模型",L"发送给 AI",L"关闭"};
    int ids[3]={CHAT_READ,CHAT_SEND,CHAT_CLOSE};
    const wchar_t *top_labels[5]={L"插件功能",L"公司标准",L"工作目录",L"预留 2",L"预留 3"};
    wchar_t slot[12], title[32]=L"BWTL";
    DWORD slot_length=GetEnvironmentVariableW(L"BWTL_INSTANCE_NUMBER",slot,_countof(slot));
    if(slot_length && slot_length<_countof(slot) && wcsspn(slot,L"0123456789")==slot_length)
        swprintf_s(title,_countof(title),L"BWTL · 会话 %ls",slot);
    if(native_window){if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);return 1;}
    cls.lpfnWndProc=native_window_proc;cls.hInstance=module;cls.lpszClassName=L"CreoCodexNativeChatV11";
    cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    cls.lpszClassName=L"CreoCodexNativeChatV14";
    native_background=CreateSolidBrush(RGB(248,250,252));cls.hbrBackground=native_background;
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    native_window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,title,WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,980,820,owner,NULL,module,NULL);
    if(!native_window)return 0;
    native_title=CreateWindowW(L"STATIC",title,WS_CHILD|WS_VISIBLE,0,0,0,0,native_window,NULL,module,NULL);
    native_mode_control=CreateWindowW(L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,0,0,0,0,native_window,(HMENU)CHAT_MODE,module,NULL);
    SendMessageW(native_mode_control,CB_ADDSTRING,0,(LPARAM)L"智能协作（按需建模）");SendMessageW(native_mode_control,CB_ADDSTRING,0,(LPARAM)L"只分析（不修改模型）");SendMessageW(native_mode_control,CB_ADDSTRING,0,(LPARAM)L"功能开发（升级插件）");SendMessageW(native_mode_control,CB_SETCURSEL,0,0);
    for(i=0;i<5;i++){
        DWORD style=WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|(i<3?WS_TABSTOP:WS_DISABLED);
        native_top_buttons[i]=CreateWindowW(L"BUTTON",top_labels[i],style,
            0,0,0,0,native_window,(HMENU)(INT_PTR)(CHAT_FEATURES+i),module,NULL);
        if(!native_top_buttons[i]){DestroyWindow(native_window);return 0;}
    }
    native_stop_button=CreateWindowW(L"BUTTON",L"停止",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_STOP,module,NULL);
    native_preview=CreateWindowW(L"STATIC",L"",WS_CHILD|SS_OWNERDRAW,0,0,0,0,native_window,(HMENU)CHAT_PREVIEW,module,NULL);
    DragAcceptFiles(native_window,TRUE);
    native_rich_module=LoadLibraryW(L"Msftedit.dll");
    if(!native_rich_module){DestroyWindow(native_window);return 0;}
    native_history=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    native_prompt=CreateWindowExW(WS_EX_CLIENTEDGE,L"RICHEDIT50W",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_WANTRETURN|ES_AUTOVSCROLL,
        0,0,0,0,native_window,NULL,module,NULL);
    DragAcceptFiles(native_prompt,TRUE);
    for(i=0;i<3;i++)native_buttons[i]=CreateWindowW(L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,native_window,(HMENU)(INT_PTR)ids[i],module,NULL);
    native_apply_button=CreateWindowW(L"BUTTON",L"应用已验升级",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
        0,0,0,0,native_window,(HMENU)CHAT_APPLY,module,NULL);
    if(!native_history||!native_prompt||!native_title||!native_mode_control||!native_preview||!native_stop_button||!native_apply_button||
        !native_buttons[0]||!native_buttons[1]||!native_buttons[2]){DestroyWindow(native_window);return 0;}
    SendMessageW(native_history,EM_EXLIMITTEXT,0,150000);
    dc=GetDC(native_window);dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(native_window,dc);
    native_font=CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    native_title_font=CreateFontW(-MulDiv(17,dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(native_title,WM_SETFONT,(WPARAM)native_title_font,TRUE);
    SendMessageW(native_history,EM_SETBKGNDCOLOR,0,RGB(232,232,232));
    SendMessageW(native_prompt,EM_SETBKGNDCOLOR,0,RGB(245,245,245));
    SendMessageW(native_history,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_prompt,WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<3;i++)SendMessageW(native_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    SetWindowPos(native_apply_button,native_buttons[0],0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SendMessageW(native_apply_button,WM_SETFONT,(WPARAM)native_font,TRUE);
    for(i=0;i<5;i++)SendMessageW(native_top_buttons[i],WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_mode_control,WM_SETFONT,(WPARAM)native_font,TRUE);
    SendMessageW(native_stop_button,WM_SETFONT,(WPARAM)native_font,TRUE);
    EnableWindow(native_stop_button,FALSE);
    SendMessageW(native_history,EM_SETLIMITTEXT,12000,0);SendMessageW(native_prompt,EM_SETLIMITTEXT,6000,0);
    native_prompt_original=(WNDPROC)SetWindowLongPtrW(native_prompt,GWLP_WNDPROC,(LONG_PTR)native_prompt_proc);
    native_layout();if(show)ShowWindow(native_window,SW_SHOWNOACTIVATE);
    return 1;
}
