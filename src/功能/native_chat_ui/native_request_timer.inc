/* UI-thread-owned monotonic clock. Continuations share the accepted request's clock. */
static ULONGLONG native_request_started, native_request_elapsed;
static int native_request_running, native_request_measured;

static void native_request_timer_format(wchar_t *text,size_t capacity,int running,ULONGLONG elapsed){
    ULONGLONG seconds=elapsed/1000, tenths=(elapsed%1000)/100;
    if(seconds>=3600)
        swprintf_s(text,capacity,L"%ls %llu时%02llu分%02llu.%llu秒",running?L"运行中":L"用时",seconds/3600,(seconds/60)%60,seconds%60,tenths);
    else if(seconds>=60)
        swprintf_s(text,capacity,L"%ls %llu分%02llu.%llu秒",running?L"运行中":L"用时",seconds/60,seconds%60,tenths);
    else swprintf_s(text,capacity,L"%ls %llu.%llu 秒",running?L"运行中":L"用时",seconds,tenths);
}
static void native_request_timer_render(void){
    wchar_t text[80];
    if(!native_elapsed_label)return;
    if(!native_request_measured){SetWindowTextW(native_elapsed_label,L"等待指令");return;}
    if(native_request_running)native_request_elapsed=GetTickCount64()-native_request_started;
    native_request_timer_format(text,_countof(text),native_request_running,native_request_elapsed);
    SetWindowTextW(native_elapsed_label,text);
}
static void native_request_timer_start_ui(void){
    if(native_request_running)return;
    native_request_started=GetTickCount64();native_request_elapsed=0;
    native_request_running=native_request_measured=1;
    SetTimer(native_window,CHAT_TIME_TICK,100,NULL);
    native_request_timer_render();
}
static void native_request_timer_finish_ui(void){
    if(!native_request_running)return;
    native_request_elapsed=GetTickCount64()-native_request_started;
    native_request_running=0;
    KillTimer(native_window,CHAT_TIME_TICK);
    native_request_timer_render();
}
static void native_request_timer_begin(void){
    if(native_window)SendMessageW(native_window,CHAT_TIME_BEGIN,0,0);
}
static void native_request_timer_finish(void){
    if(native_window)SendMessageW(native_window,CHAT_TIME_END,0,0);
}