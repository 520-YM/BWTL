/* Run the production chat and its real AI engine in a disposable Creo session. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ULONGLONG demo_started;
static int demo_queued, demo_allowed, demo_analysis_blocked, demo_upgrade_removed;

static int demo_end(int passed){
    FILE *log=fopen("direct-chat-result.log","a");
    if(log){
        fprintf(log,"DIRECT_CHAT_NATIVE_%s\n",passed?"PASS":"FAIL");
        fclose(log);
    }
    KillTimer(native_backend_window,5001);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static void CALLBACK demo_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!demo_queued)demo_queued=ProMacroLoad(L"~ Command `BWTLDirectChatTick`;")==PRO_TK_NO_ERROR;
}

static int demo_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    ProFeature feature;
    ProGeomitem geometry;
    ProCurve curve;
    ProEnttype kind;
    int physical_spline=0,single_reply=0;
    FILE *log;
    const wchar_t *first_reply;
    demo_queued=0;
    if(GetTickCount64()-demo_started>330000)return demo_end(0);
    if(smart_active||ai_process)return 0;
    first_reply=wcsstr(history,L"【AI】");
    single_reply=first_reply&&!wcsstr(first_reply+4,L"【AI】")&&!wcsstr(first_reply,L"宿主标记为只读");
    if(!ProMdlCurrentGet(&model)&&!part_health(model)&&
       !ProModelitemByNameInit(model,PRO_FEATURE,L"DEMO_SPLINE",(ProModelitem *)&feature)&&
       !ad_single_curve(&feature,&geometry,&curve)&&!ProCurveTypeGet(curve,&kind))
        physical_spline=kind==PRO_ENT_SPLINE||kind==PRO_ENT_B_SPLINE;
    log=fopen("direct-chat-result.log","a");
    if(log){
        fprintf(log,"execution_allowed=%d analysis_blocks_write=%d old_upgrade_command_removed=%d actual_spline=%d steps=%d failures=%d one_reply=%d\n",
            demo_allowed,demo_analysis_blocked,demo_upgrade_removed,physical_spline,smart_steps,smart_failures,single_reply);
        fclose(log);
    }
    return demo_end(demo_allowed&&demo_analysis_blocked&&demo_upgrade_removed&&physical_spline&&single_reply&&smart_steps>=2);
}

static int demo_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    if(!ProMdlCurrentGet(&model))return demo_end(0);
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0)return demo_end(0);
    CloseHandle(native_ui_ready);native_ui_ready=NULL;
    native_prompt_set(L"应用已验升级");
    run_local(NULL,NULL,NULL);
    demo_upgrade_removed=!smart_active&&!ai_process&&wcsstr(history,L"BWTL 更新在 Codex 中处理。")!=NULL;
    smart_begin(L"只分析三维样条方案，不修改模型");
    pending_plan=(ModelPlan){0};
    pending_plan.ready=1;
    pending_plan.empty_kind=1;
    execute_model_plan();
    demo_analysis_blocked=!smart_active&&!pending_plan.ready&&ProMdlCurrentGet(&model)!=PRO_TK_NO_ERROR;
    history[0]=0;
    wcscpy_s(conversation,_countof(conversation),
        L"用户：准备演示第39项三维样条。先新建20×20×20毫米的无孔示例件，再用样件实际坐标系创建 DEMO_SPLINE，"
        L"依次经过(0,0,0)、(8,12,3)、(17,-2,11)、(28,10,19)，不保存。\n"
        L"助手：当前这轮仍被宿主标记为只读，尚未创建模型。\n");
    demo_started=GetTickCount64();
    if(!SetTimer(native_backend_window,5001,1000,demo_timer))return demo_end(0);
    native_prompt_set(L"刚刚增加的第39项功能，演示给我看。");
    run_local(NULL,NULL,NULL);
    demo_allowed=smart_active&&!smart_readonly&&!request_requires_current;
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError e;
    uiCmdCmdId command;
    wchar_t codex[MAX_PATH];
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=bwtl_production_initialize(argc,argv,version,build,error_buffer)))return e;
    if(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings))
        return PRO_TK_E_NOT_FOUND;
    if((e=ProCmdActionAdd("BWTLDirectChatBegin",demo_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command))||
       (e=ProCmdActionAdd("BWTLDirectChatTick",demo_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLDirectChatBegin`;");
}
