#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProMdl target;
static ULONGLONG started;
static int queued;

static int finish_execution_test(int pass){
    KillTimer(native_backend_window,9030);
    FILE *log=fopen("execution-chat.log","w");
    if(log){fprintf(log,"NATIVE_WRITES=%d FOLLOWUP_EXECUTED=%d\n",smart_progress,pass);fclose(log);}
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int verify_execution(uiCmdCmdId id,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()-started>120000)return finish_execution_test(0);
    if(smart_active||ai_process)return 0;
    ProMdl active=NULL;
    ProFeature datum;
    const wchar_t *reply=wcsstr(history,L"【AI】");
    return finish_execution_test(!ProMdlCurrentGet(&active)&&active==target&&smart_progress==1&&
        !ProModelitemByNameInit(target,PRO_FEATURE,L"CHECK_OFFSET",&datum)&&reply&&!wcsstr(reply+4,L"【AI】"));
}

static void CALLBACK execution_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!queued)queued=ProMacroLoad(L"~ Command `BWTLExecutionVerify`;")==PRO_TK_NO_ERROR;
}

static int send_execution(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl active=NULL;
    if(ProMdlCurrentGet(&active)||active!=target||!native_ui_open_threaded(NULL)||
        !SetTimer(native_backend_window,9030,1000,execution_timer))return finish_execution_test(0);
    wcscpy_s(conversation,_countof(conversation),
        L"用户：在当前零件上新建一个与 FRONT 平行、偏置 10 毫米的基准面，命名 CHECK_OFFSET。\n"
        L"助手：方案明确，可以使用 FRONT 作参照创建 CHECK_OFFSET。\n");
    native_prompt_set(L"你做吧，做到哪步算哪步。");
    started=GetTickCount64();
    run_local(NULL,NULL,NULL);
    return 0;
}

static int start_execution(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdlName name;
    Fixture stock;
    model_background_mode=1;
    ProError error=fixture(&target,name,&stock);
    model_background_mode=0;
    if(!error)error=dv_display_new(target);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLExecutionSend`;");
    return error?finish_execution_test(0):0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    wchar_t codex[MAX_PATH];
    if(!error&&(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)))
        error=PRO_TK_E_NOT_FOUND;
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLExecutionStart",start_execution,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLExecutionSend",send_execution,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLExecutionVerify",verify_execution,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLExecutionStart`;");
}