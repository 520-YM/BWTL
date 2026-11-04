static int native_ui_open_threaded(HWND owner){
    if(!native_ui_thread){
        native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);if(!native_ui_ready)return 0;
        native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);if(!native_ui_thread)return 0;
        if(WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0)return 0;
        CloseHandle(native_ui_ready);native_ui_ready=NULL;
    }
    if(!IsWindow(native_window))return 0;
    if(owner)SetWindowLongPtrW(native_window,GWLP_HWNDPARENT,(LONG_PTR)owner);
    ShowWindowAsync(native_window,SW_SHOWNOACTIVATE);return 1;
}
