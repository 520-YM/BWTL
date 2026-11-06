/* Exercise real Creo directory APIs without touching the user's clipboard or Explorer. */
#include <windows.h>
#include <shellapi.h>
#include <ProToolkit.h>
#include <ProUtil.h>
static ProError probe_choose(ProName label,ProPath *paths,ProName *names,ProPath current,ProPath selected);
static HINSTANCE WINAPI probe_explorer(HWND owner,LPCWSTR verb,LPCWSTR path,LPCWSTR args,LPCWSTR directory,INT show);
static BOOL WINAPI probe_clipboard_open(HWND owner);
static BOOL WINAPI probe_clipboard_empty(void);
static BOOL WINAPI probe_clipboard_close(void);
static HANDLE WINAPI probe_clipboard_set(UINT format,HANDLE handle);
#define ProDirectoryChoose probe_choose
#define ShellExecuteW probe_explorer
#define OpenClipboard probe_clipboard_open
#define EmptyClipboard probe_clipboard_empty
#define CloseClipboard probe_clipboard_close
#define SetClipboardData probe_clipboard_set
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#undef ProDirectoryChoose
#undef ShellExecuteW
#undef OpenClipboard
#undef EmptyClipboard
#undef CloseClipboard
#undef SetClipboardData
#include <ProCore.h>

static ProPath original_dir,selected_dir,log_path;
static ProMdl probe_model;
static ProWVerstamp probe_stamp;
static int phase,queued,chooser_count,opened,copied;
static ULONGLONG started;

static ProError probe_choose(ProName label,ProPath *paths,ProName *names,ProPath current,ProPath selected){
    chooser_count++;
    if(chooser_count==3)return ProDirectoryChoose(label,paths,names,current,selected);
    if(native_busy!=2||_wcsicmp(current,original_dir))return PRO_TK_BAD_CONTEXT;
    if(chooser_count==1)return PRO_TK_USER_ABORT;
    wcscpy_s(selected,PRO_PATH_SIZE,selected_dir);
    return PRO_TK_NO_ERROR;
}
static HINSTANCE WINAPI probe_explorer(HWND owner,LPCWSTR verb,LPCWSTR path,LPCWSTR args,LPCWSTR directory,INT show){
    opened=!wcscmp(verb,L"open")&&!_wcsicmp(path,original_dir);
    return (HINSTANCE)(INT_PTR)(opened?33:2);
}
static BOOL WINAPI probe_clipboard_open(HWND owner){return TRUE;}
static BOOL WINAPI probe_clipboard_empty(void){return TRUE;}
static BOOL WINAPI probe_clipboard_close(void){return TRUE;}
static HANDLE WINAPI probe_clipboard_set(UINT format,HANDLE handle){
    const wchar_t *value=GlobalLock(handle);
    copied=format==CF_UNICODETEXT&&value&&!_wcsicmp(value,original_dir);
    if(value)GlobalUnlock(handle);
    if(copied){GlobalFree(handle);return (HANDLE)(INT_PTR)1;}
    return NULL;
}
static void probe_log(const char *label,int passed){
    FILE *f=_wfopen(log_path,L"a");
    if(f){fprintf(f,"%s: %s\n",label,passed?"PASS":"FAIL");fclose(f);}
}
static int probe_end(int passed){
    probe_log("BWTL_WORKDIR_NATIVE",passed);
    if(probe_stamp)ProVerstampFree(&probe_stamp);
    if(original_dir[0])ProDirectoryChange(original_dir);
    KillTimer(native_backend_window,5001);
    ProEngineerEnd();
    return 0;
}
static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!queued)queued=ProMacroLoad(L"~ Command `BWTLWorkdirProbeTick`;")==PRO_TK_NO_ERROR;
}
static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProPath actual;
    ProMdl model;ProWVerstamp stamp=NULL;int unchanged;
    queued=0;
    if(GetTickCount64()-started>160000||ProDirectoryCurrentGet(actual))return probe_end(0);
    switch(phase++){
    case 0:
        if(ProMdlCurrentGet(&model)||model!=probe_model||ProMdlVerstampGet(model,&probe_stamp))return probe_end(0);
        native_set_busy(1);native_workdir_action(WORKDIR_PICK);return 0;
    case 1:
        if(chooser_count||_wcsicmp(actual,original_dir))return probe_end(0);
        probe_log("busy task blocks directory changes",1);
        native_set_busy(0);native_workdir_action(WORKDIR_VIEW);return 0;
    case 2:
        if(!wcsstr(history,L"Creo 当前工作目录")||!wcsstr(history,original_dir))return probe_end(0);
        probe_log("view reads actual Creo current directory",1);
        native_workdir_action(WORKDIR_OPEN);return 0;
    case 3:
        if(!opened)return probe_end(0);
        probe_log("Explorer receives the current path",1);
        native_workdir_action(WORKDIR_COPY);return 0;
    case 4:
        if(!copied)return probe_end(0);
        probe_log("clipboard receives the complete Unicode path",1);
        native_workdir_action(WORKDIR_PICK);return 0;
    case 5:
        if(chooser_count!=1||native_busy||_wcsicmp(actual,original_dir))return probe_end(0);
        probe_log("cancel keeps the current directory",1);
        smart_waiting_for_user=1;pending_plan.ready=1;pending_part.verb=PART_READ;
        native_workdir_action(WORKDIR_PICK);return 0;
    case 6: {
        const size_t length=wcslen(selected_dir);
        if(chooser_count!=2||native_busy||_wcsnicmp(actual,selected_dir,length)||
           (actual[length]&&(actual[length]!=L'\\'||actual[length+1]))||smart_waiting_for_user||
           pending_plan.ready||pending_part.verb!=PART_NONE||ProMdlCurrentGet(&model)||model!=probe_model||
           ProMdlVerstampGet(model,&stamp))return probe_end(0);
        unchanged=ProVerstampEqual(probe_stamp,stamp);ProVerstampFree(&stamp);
        if(!unchanged)return probe_end(0);
        probe_log("directory switch read back from Creo and active model preserved",1);
        native_workdir_action(WORKDIR_PICK);return 0;
    }
    default:
        if(native_busy&&chooser_count==3){phase=7;return 0;}
        unchanged=chooser_count==3&&!native_busy&&wcsstr(actual,L"目录验收")!=NULL;
        probe_log("real Creo folder picker cancel retains directory",unchanged);
        return probe_end(unchanged);
    }
}
static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    const char *stage="fixture";ProMdlName name;
    if(ProDirectoryCurrentGet(original_dir))return probe_end(0);
    swprintf_s(log_path,_countof(log_path),L"%ls\\workdir-result.log",original_dir);
    swprintf_s(selected_dir,_countof(selected_dir),L"%ls%ls目录验收",original_dir,original_dir[wcslen(original_dir)-1]==L'\\'?L"":L"\\");
    if(!CreateDirectoryW(selected_dir,NULL)||model_create_empty(1,name,&stage)||
       ProMdlnameInit(name,PRO_MDL_PART,&probe_model))return probe_end(0);
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0||!native_window)return probe_end(0);
    CloseHandle(native_ui_ready);native_ui_ready=NULL;
    started=GetTickCount64();
    return SetTimer(native_backend_window,5001,1000,probe_timer)?0:probe_end(0);
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error;uiCmdCmdId command;char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,16)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    error=ProCmdActionAdd("BWTLWorkdirProbeBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("BWTLWorkdirProbeTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    return ProMacroLoad(L"~ Command `BWTLWorkdirProbeBegin`;");
}
