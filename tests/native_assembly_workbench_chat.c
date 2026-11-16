#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ULONGLONG aw_chat_started;
static int aw_chat_target,aw_chat_failures,aw_chat_count,aw_chat_queued;
static ProMdl aw_chat_assembly;

static void aw_chat_check(int pass,const char *name){
    aw_chat_count++;
    aw_chat_failures+=!pass;
    FILE *log=fopen("workbench-chat.log","a");
    if(log){fprintf(log,"%s: %d\n",name,pass?0:-1);fclose(log);}
}

static int aw_chat_finish(void){
    if(native_backend_window)KillTimer(native_backend_window,6441);
    smart_cancel();
    FILE *log=fopen("workbench-chat.log","a");
    if(log){
        fprintf(log,"AW_CHAT_CASES: %d\nAW_CHAT_FAILURES: %d\nAW_CHAT_DONE: 1\n",aw_chat_count,aw_chat_failures);
        fclose(log);
    }
    ProEngineerEnd();
    return 0;
}

static int aw_chat_check_result(uiCmdCmdId id,uiCmdValue *value,void *context){
    aw_chat_queued=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-aw_chat_started<170000)return 0;
        aw_chat_check(0,"chat deadline");return aw_chat_finish();
    }
    wchar_t occurrence[32];
    swprintf_s(occurrence,_countof(occurrence),L"ID:%d",aw_chat_target);
    AxComponent actual={0};
    const wchar_t *answer=wcsstr(history,L"【AI】");
    aw_chat_check(smart_intent==SMART_INTENT_CURRENT,"natural request chooses current assembly modeling");
    aw_chat_check(wcsstr(smart_details,L"ASM_W_ALIGN ")&&wcsstr(smart_details,L"第 2 组"),"chat uses native fallback");
    aw_chat_check(!ax_component_resolve(aw_chat_assembly,occurrence,&actual)&&
        aw_aligned(aw_chat_assembly,&actual,L"CSYS:ASM_CSYS",L"CSYS:PRT_CSYS_DEF",1),"native placement independently confirmed");
    aw_chat_check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"未保存"),"one final reply with unsaved disposition");
    return aw_chat_finish();
}

static void CALLBACK aw_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!aw_chat_queued)aw_chat_queued=!ProMacroLoad(L"~ Command \x0060" L"BWTLWorkbenchChatCheck\x0060;");
}

static int aw_chat_dispatch(uiCmdCmdId id,uiCmdValue *value,void *context){
    wchar_t exe[MAX_PATH],request[760];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
        !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
        !SetTimer(native_backend_window,6441,500,aw_chat_timer)){
        aw_chat_check(0,"private chat initialization");return aw_chat_finish();
    }
    const int len=swprintf_s(request,_countof(request),L"在当前装配里直接执行「装配失败后自动换参照」，把未定位组件 ID:%d 对齐。"
        L"第一组总装坐标系 PRIMARY_TEST 配零件 PRT_CSYS_DEF，如果没法执行就换总装 ASM_CSYS 配零件 PRT_CSYS_DEF。"
        L"最多移动 40 毫米、转动 1 度。保留同一组件，不保存。",aw_chat_target);
    if(len<0)return aw_chat_finish();
    aw_chat_started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int aw_chat_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProMdl model=NULL;
    ProMdlName name=L"WB_CHAT_BRACKET",root;
    ModelPlan plan={0,20,20,8,0,1};
    const char *stage="";
    ProAsmcomp component;
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{25,0,0,1}};
    model_background_mode=0;
    ProError error=quick_seed(name,&model);
    if(!error)error=model_build_internal(&plan,name,&stage,model,NULL,NULL);
    if(!error)error=model_create_empty(3,root,&stage);
    if(!error)error=ProMdlnameInit(root,PRO_MDL_ASSEMBLY,&aw_chat_assembly);
    if(!error)error=ProAsmcompAssemble((ProAssembly)aw_chat_assembly,(ProSolid)model,pose,&component);
    if(!error)error=part_regenerate(aw_chat_assembly);
    if(!error){
        aw_chat_target=component.id;
        error=ProMacroLoad(L"~ Command \x0060" L"BWTLWorkbenchChatDispatch\x0060;");
    }
    if(error){trace(stage,error);aw_chat_check(0,"assembled independent test stock");return aw_chat_finish();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLWorkbenchChatPrepare",aw_chat_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLWorkbenchChatDispatch",aw_chat_dispatch,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLWorkbenchChatCheck",aw_chat_check_result,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(error)return error;
    return ProMacroLoad(L"~ Command \x0060" L"BWTLWorkbenchChatPrepare\x0060;");
}
