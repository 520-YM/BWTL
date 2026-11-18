#define user_initialize timer_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>
static FILE *clock_log;
static int clock_failures;
static void clock_check(int pass,const char *name){
 if(!pass)clock_failures++;
 fprintf(clock_log,"%s=%d\n",name,!!pass);fflush(clock_log);
}
static VOID CALLBACK clock_verify_later(HWND window,UINT message,UINT_PTR id,DWORD tick){
 KillTimer(window,id);
 ProMacroLoad(L"~ Command `BWTLTimerVerify`;");
}
static int clock_verify(uiCmdCmdId command,uiCmdValue *value,void *data){
 wchar_t label[80];GetWindowTextW(native_elapsed_label,label,_countof(label));
 clock_check(!native_request_running&&wcsstr(label,L"用时")&&native_request_started>0,"SEND_BUTTON_LOCAL_COMMAND_FINISHED");
 clock_check(wcsstr(history,L"计时独立会话")!=NULL,"CHAT_RESULT_PRESERVED");
 fprintf(clock_log,"REQUEST_TIMER_NATIVE_PASS=%d\n",!clock_failures);fclose(clock_log);
 ProEngineerEnd();return 0;
}
static int clock_start(uiCmdCmdId command,uiCmdValue *value,void *data){
 wchar_t label[80],frozen[80];
 clock_log=fopen("request-timer-native.log","w");
 if(!clock_log){ProEngineerEnd();return 0;}
 clock_check(open_window(0,NULL,NULL)==0,"OPEN_NATIVE_WINDOW");
 GetWindowTextW(native_elapsed_label,label,_countof(label));
 clock_check(!wcscmp(label,L"等待指令"),"IDLE_LABEL");
 native_request_timer_begin();smart_begin(L"计时独立会话测试");
 ULONGLONG first=native_request_started;
 Sleep(350);GetWindowTextW(native_elapsed_label,label,_countof(label));
 clock_check(native_request_running&&native_request_elapsed>=200&&wcsstr(label,L"运行中"),"LIVE_UPDATES_DURING_HOST_WORK");
 native_set_busy(2);native_set_busy(1);native_request_timer_begin();
 clock_check(native_request_started==first,"CONTINUATION_DOES_NOT_RESET");
 smart_reply(L"计时独立会话：正常完成。");GetWindowTextW(native_elapsed_label,frozen,_countof(frozen));
 clock_check(!native_request_running&&wcsstr(frozen,L"用时"),"SUCCESS_FREEZES_TOTAL");
 Sleep(150);GetWindowTextW(native_elapsed_label,label,_countof(label));
 clock_check(!wcscmp(frozen,label),"FINAL_TOTAL_STAYS_FIXED");
 smart_begin(L"计时失败路径测试");
 clock_check(native_request_running&&native_request_started>first,"NEXT_COMMAND_RESETS");
 smart_pause(L"计时独立会话：失败路径测试",0);
 clock_check(!native_request_running&&!smart_active,"FAILURE_STOPS_CLOCK");
 smart_begin(L"计时停止路径测试");Sleep(110);smart_stop();
 clock_check(!native_request_running&&!smart_active,"USER_STOP_STOPS_CLOCK");
 native_prompt_set(L"查看方案");run_local(NULL,NULL,NULL);
 clock_check(!native_request_running&&!smart_active&&!ai_process,"SYNCHRONOUS_LOCAL_COMMAND_ENDS");
 for(int dpi=96;dpi<=144;dpi+=48){
  native_dpi=dpi;SetWindowPos(native_window,NULL,0,0,MulDiv(640,dpi,96),MulDiv(650,dpi,96),SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
  RECT title,timer;GetWindowRect(native_title,&title);GetWindowRect(native_elapsed_label,&timer);
  clock_check(title.right<timer.left&&timer.top>=title.top,dpi==96?"NARROW_HEADER_LAYOUT":"DPI_HEADER_LAYOUT");
 }
 native_dpi=96;
 native_prompt_set(L"查看方案");SendMessageW(native_window,WM_COMMAND,CHAT_SEND,0);
 GetWindowTextW(native_elapsed_label,label,_countof(label));
 clock_check(native_request_running&&wcsstr(label,L"运行中"),"SEND_BUTTON_STARTS_IMMEDIATELY");
 if(!SetTimer(native_backend_window,9417,500,clock_verify_later)){
  clock_check(0,"VERIFY_TIMER_CREATED");fclose(clock_log);ProEngineerEnd();
 }
 return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
 if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
 ProError error=timer_production_init(argc,argv,version,build,buffer);
 uiCmdCmdId command;
 if(!error)error=ProCmdActionAdd("BWTLTimerVerify",clock_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
 if(!error)error=ProCmdActionAdd("BWTLTimerStart",clock_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
 return error?error:ProMacroLoad(L"~ Command `BWTLTimerStart`;");
}