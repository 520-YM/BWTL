
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int quality_chat_failures,quality_chat_cases;
static ULONGLONG quality_chat_started;
static int quality_chat_ids[2];

static void quality_chat_check(int pass,const char *label){
    quality_chat_cases++;
    if(!pass)quality_chat_failures++;
    FILE *f=fopen("quality-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int quality_chat_done(void){
    if(native_backend_window)KillTimer(native_backend_window,5441);
    smart_cancel();
    FILE *f=fopen("quality-chat.log","a");
    if(f){fprintf(f,"QUALITY_CHAT_CASES: %d\nQUALITY_CHAT_FAILURES: %d\nQUALITY_CHAT_DONE: 1\n",
        quality_chat_cases,quality_chat_failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static int quality_chat_tick(uiCmdCmdId id,uiCmdValue *value,void *context){
    if(smart_active||ai_process){
        if(GetTickCount64()-quality_chat_started<250000)return 0;
        quality_chat_check(0,"native chat timed out");return quality_chat_done();
    }
    quality_chat_check(smart_intent==SMART_INTENT_READ,"Codex uses read-only intent");
    quality_chat_check(smart_steps==2&&smart_progress==0&&wcsstr(smart_details,L"ASM_Q_GAPS ")&&
        wcsstr(smart_details,L"ASM_Q_REPORT "),"check and report execute in one chat turn");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    quality_chat_check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"装配质量检查报告")&&
        wcsstr(answer,L"最小间隙 1 mm")&&wcsstr(answer,L"不符合"),"single native report contains measured 1 mm and failed limit");
    return quality_chat_done();
}

static void CALLBACK quality_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    ProMacroLoad(L"~ Command `BWTLQualityChatTick`;");
}

static int quality_chat_request(uiCmdCmdId id,uiCmdValue *value,void *context){
    wchar_t exe[MAX_PATH],request[600];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
        !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
        !SetTimer(native_backend_window,5441,1000,quality_chat_timer)){
        quality_chat_check(0,"start private Codex quality chat");return quality_chat_done();
    }
    swprintf_s(request,_countof(request),
        L"在当前装配，先按第312项检查屏幕区域 ID:%d 和 ID:%d 的最小间隙，要求至少2 mm，然后用第320项把装配检查报告发在聊天里。只读取。",
        quality_chat_ids[0],quality_chat_ids[1]);
    quality_chat_started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int quality_chat_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProMdl part=NULL,root=NULL;
    ProMdlName name=L"QUALITY_CHAT_STOCK",assembly;
    ModelPlan plan={0,20,20,8,0,1};
    const char *stage="quality chat stock";
    model_background_mode=0;
    ProError error=quick_seed(name,&part);
    if(!error)error=model_build_internal(&plan,name,&stage,part,NULL,NULL);
    if(!error)error=model_create_empty(3,assembly,&stage);
    if(!error)error=ProMdlnameInit(assembly,PRO_MDL_ASSEMBLY,&root);
    for(int i=0;!error&&i<2;i++){
        ProAsmcomp component;
        ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{21*i,0,0,1}};
        error=ProAsmcompAssemble((ProSolid)root,part,pose,&component);
        if(!error)quality_chat_ids[i]=component.id;
    }
    if(!error)error=part_regenerate(root);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLQualityChatRequest`;");
    if(error){trace(stage,error);quality_chat_check(0,"quality chat fixture");return quality_chat_done();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLQualityChatPrepare",quality_chat_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLQualityChatRequest",quality_chat_request,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLQualityChatTick",quality_chat_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLQualityChatPrepare`;");
}
