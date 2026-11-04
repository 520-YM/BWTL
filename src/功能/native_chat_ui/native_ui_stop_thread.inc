static void native_ui_stop_thread(void){
    if(native_window)SendMessageW(native_window,CHAT_DESTROY,0,0);
    if(native_ui_thread){WaitForSingleObject(native_ui_thread,5000);CloseHandle(native_ui_thread);native_ui_thread=NULL;}
    native_ui_thread_id=0;
}
