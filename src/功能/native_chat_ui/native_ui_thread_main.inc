static DWORD WINAPI native_ui_thread_main(void *unused){
    MSG message;
    native_ui_thread_id=GetCurrentThreadId();
    native_ui_create(NULL,0);SetEvent(native_ui_ready);
    if(native_window)while(GetMessageW(&message,NULL,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    UnregisterClassW(L"CreoCodexNativeChatV14",GetModuleHandleW(NULL));
    if(native_title_font){DeleteObject(native_title_font);native_title_font=NULL;}
    if(native_background){DeleteObject(native_background);native_background=NULL;}
    if(native_rich_module){FreeLibrary(native_rich_module);native_rich_module=NULL;}
    return 0;
}
