
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int views_chat_failures,views_chat_cases;
static ULONGLONG views_chat_started;
static int views_chat_ids[2];

static void views_chat_check(int pass,const char *label){
    views_chat_cases++;
    if(!pass)views_chat_failures++;
    FILE *f=fopen("views-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int views_chat_done(void){
    if(native_backend_window)KillTimer(native_backend_window,5441);
    smart_cancel();
    FILE *f=fopen("views-chat.log","a");
    if(f){fprintf(f,"VIEWS_CHAT_CASES: %d\nVIEWS_CHAT_FAILURES: %d\nVIEWS_CHAT_DONE: 1\n",
        views_chat_cases,views_chat_failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static int views_chat_tick(uiCmdCmdId id,uiCmdValue *value,void *context){
    if(smart_active||ai_process){
        if(GetTickCount64()-views_chat_started<250000)return 0;
        views_chat_check(0,"native chat timed out");return views_chat_done();
    }
    views_chat_check(smart_intent==SMART_INTENT_CURRENT&&!smart_readonly,"current assembly write intent");
    ProMdl model=NULL;
    ProSimprep active={0},expected={0};
    views_chat_check(smart_steps==1&&wcsstr(smart_details,L"ASM_V_ISOLATE ")&&
        !ProMdlCurrentGet(&model)&&!ProSimprepActiveGet((ProSolid)model,&active)&&
        !ax_view_rep_lookup(model,L"ONLY_POWER",&expected)&&active.id==expected.id,
        "native isolation request activated the requested representation");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    views_chat_check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"ONLY_POWER"),"single final result");
    return views_chat_done();
}

static void CALLBACK views_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    ProMacroLoad(L"~ Command `BWTLViewsChatTick`;");
}

static int views_chat_request(uiCmdCmdId id,uiCmdValue *value,void *context){
    wchar_t exe[MAX_PATH],request[600];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
        !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
        !SetTimer(native_backend_window,5441,1000,views_chat_timer)){
        views_chat_check(0,"start private Codex quality chat");return views_chat_done();
    }
    swprintf_s(request,_countof(request),
        L"请在当前装配用第322项隔离显示组件 ID:%d，隔离表示命名 ONLY_POWER，现在执行并保持隔离显示。",
        views_chat_ids[0]);
    views_chat_started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int views_chat_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProMdl part=NULL,root=NULL;
    ProMdlName name=L"VIEWS_CHAT_STOCK",assembly;
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
        if(!error)views_chat_ids[i]=component.id;
    }
    if(!error)error=part_regenerate(root);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLViewsChatRequest`;");
    if(error){trace(stage,error);views_chat_check(0,"quality chat fixture");return views_chat_done();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLViewsChatPrepare",views_chat_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLViewsChatRequest",views_chat_request,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLViewsChatTick",views_chat_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLViewsChatPrepare`;");
}
