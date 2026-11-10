/* Exact reported chat request through the production assistant and real AI. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ULONGLONG probe_started;
static int probe_queued,probe_route;

static int probe_end(int passed){
    FILE *log=fopen("create-then-shell-result.log","a");
    if(log){fprintf(log,"CREATE_THEN_SHELL_NATIVE_%s\n",passed?"PASS":"FAIL");fclose(log);}
    KillTimer(native_backend_window,5001);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static ProError probe_shell(ProFeature *feature,ProError status,ProAppData data){
    ProFeattype type;
    if(status)return status;
    if(!ProFeatureTypeGet(feature,&type)&&type==PRO_FEAT_SHELL){
        *(ProFeature *)data=*feature;
        return PRO_TK_E_FOUND;
    }
    return PRO_TK_NO_ERROR;
}

static int probe_geometry(ProMdl model,double *thickness,double *volume){
    const ProElemId path[]={PRO_E_SHELL_THICK};
    ProFeature shell;
    ProElement tree=NULL,element=NULL;
    ProMassProperty mass;
    Pro3dPnt outline[2];
    double r,h,width,expected,pi=3.141592653589793;
    int bodies=0;
    ProError e;
    if(part_health(model)||sd_body_count(model,&bodies)||bodies!=1||
       ProSolidFeatVisit((ProSolid)model,probe_shell,NULL,&shell)!=PRO_TK_E_FOUND||
       ProSolidOutlineGet((ProSolid)model,outline)||
       ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass))return 0;
    if((e=ProFeatureElemtreeExtract(&shell,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)))return 0;
    e=mc_tree_element(tree,path,1,&element);
    if(!e)e=ProElementDoubleGet(element,NULL,thickness);
    ProFeatureElemtreeFree(&shell,tree);
    if(e)return 0;
    *volume=mass.volume;
    width=outline[1][0]-outline[0][0];r=width/2;h=outline[1][2]-outline[0][2];
    if(!part_close(width,outline[1][1]-outline[0][1])||*thickness<=0||*thickness>=fmin(r,h))return 0;
    expected=pi*(r*r*h-(r-*thickness)*(r-*thickness)*(h-*thickness));
    return part_close(*volume,expected);
}

static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!probe_queued)probe_queued=ProMacroLoad(L"~ Command `BWTLCreateThenShellTick`;")==PRO_TK_NO_ERROR;
}

static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    double thickness=0,volume=0;
    int geometry=0,one_reply=0,bound=0;
    const wchar_t *reply;
    FILE *log;
    probe_queued=0;
    if(GetTickCount64()-probe_started>330000)return probe_end(0);
    if(smart_active||ai_process)return 0;
    if(!ProMdlCurrentGet(&model)){
        geometry=probe_geometry(model,&thickness,&volume);
        bound=model==smart_target_model&&request_requires_current;
    }
    reply=wcsstr(history,L"【AI】");
    one_reply=reply&&!wcsstr(reply+4,L"【AI】");
    log=fopen("create-then-shell-result.log","a");
    if(log){
        fprintf(log,"new_model_allowed=%d bound_to_new_model=%d actual_native_shell=%d thickness=%g volume=%g steps=%d failures=%d one_reply=%d\n",
            probe_route,bound,geometry,thickness,volume,smart_steps,smart_failures,one_reply);
        fclose(log);
    }
    return probe_end(probe_route&&bound&&geometry&&one_reply&&smart_steps>=2&&!smart_failures);
}

static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    if(!ProMdlCurrentGet(&model))return probe_end(0);
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0)return probe_end(0);
    CloseHandle(native_ui_ready);native_ui_ready=NULL;
    probe_started=GetTickCount64();
    if(!SetTimer(native_backend_window,5001,1000,probe_timer))return probe_end(0);
    native_prompt_set(L"画一个圆柱，抽壳。数据自己给。");
    run_local(NULL,NULL,NULL);
    probe_route=smart_active&&!smart_readonly&&!request_requires_current;
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
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings))return PRO_TK_E_NOT_FOUND;
    if((e=ProCmdActionAdd("BWTLCreateThenShellBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command))||
       (e=ProCmdActionAdd("BWTLCreateThenShellTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLCreateThenShellBegin`;");
}
