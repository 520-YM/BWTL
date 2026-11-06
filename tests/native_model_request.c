/* Exercise the ordinary chat send path with a real engine and a fresh Creo session. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ULONGLONG probe_started;
static int probe_queued, probe_write_allowed;
static wchar_t probe_last_protocol[8192];
static const wchar_t probe_request[] = L"现在可以帮我做水杯了吗？你自己去选择调用功能、自己有想法去做、尺寸你自己给。";

static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!probe_queued)probe_queued=ProMacroLoad(L"~ Command `BWTLModelRequestTick`;")==PRO_TK_NO_ERROR;
}

static ProError probe_cavity_count(ProFeature *feature,ProError status,ProAppData data){
    ProFeattype type;
    if(status)return status;
    if(!ProFeatureTypeGet(feature,&type)&&(type==PRO_FEAT_HOLE||type==PRO_FEAT_CUT))
        (*(int *)data)++;
    return PRO_TK_NO_ERROR;
}

static int probe_end(int passed){
    FILE *result=fopen("model-request-native-result.log","a");
    if(result){fprintf(result,"MODEL_REQUEST_NATIVE_%s\n",passed?"PASS":"FAIL");fclose(result);}
    KillTimer(native_backend_window,5001);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    ProMassProperty mass={0};
    Pro3dPnt outline[2];
    int cavity=0,bodies=0,solid_ok=0,guarded=0,one_reply=0;
    double pi=3.141592653589793,expected=pi*(40.0*40.0*100.0-37.0*37.0*95.0);
    FILE *result;
    probe_queued=0;
    if(GetTickCount64()-probe_started>350000)return probe_end(0);
    if(wcscmp(probe_last_protocol,smart_last_protocol)){
        FILE *protocol=_wfopen(L"model-request-protocol.log",L"a, ccs=UTF-8");
        if(protocol){fwprintf(protocol,L"%ls\n",smart_last_protocol);fclose(protocol);}
        wcscpy_s(probe_last_protocol,_countof(probe_last_protocol),smart_last_protocol);
    }
    if(smart_active||ai_process)return 0;
    {
        const wchar_t *reply=wcsstr(history,L"【AI】");
        one_reply=reply&&!wcsstr(reply+4,L"【AI】");
    }
    if(!ProMdlCurrentGet(&model)&&!part_health(model)&&
       !ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)&&
       !sd_body_count(model,&bodies)&&!ProSolidOutlineGet((ProSolid)model,outline)){
        ProSolidFeatVisit((ProSolid)model,probe_cavity_count,NULL,&cavity);
        solid_ok=bodies==1&&cavity==1&&fabs(mass.volume-expected)<1.0&&
            fabs(outline[1][0]-outline[0][0]-80.0)<0.01&&
            fabs(outline[1][1]-outline[0][1]-80.0)<0.01&&
            fabs(outline[1][2]-outline[0][2]-100.0)<0.01;
    }
    result=fopen("model-request-native-result.log","a");
    if(result){
        fprintf(result,"write_allowed=%d steps=%d failures=%d cavity_features=%d bodies=%d volume_mm3=%.4f expected_mm3=%.4f dimensions_and_volume_ok=%d single_reply=%d\n",
            probe_write_allowed,smart_steps,smart_failures,cavity,bodies,mass.volume,expected,solid_ok,one_reply);
        fclose(result);
    }
    if(!probe_write_allowed||!solid_ok||!one_reply||smart_steps<2)return probe_end(0);
    native_mode=1;
    smart_begin(probe_request);
    pending_plan=(ModelPlan){0};
    pending_plan.ready=1;
    pending_plan.empty_kind=1;
    execute_model_plan();
    {
        ProMdl after;
        guarded=!smart_active&&!pending_plan.ready&&!ProMdlCurrentGet(&after)&&after==model;
    }
    result=fopen("model-request-native-result.log","a");
    if(result){fprintf(result,"analysis_host_blocks_new_model=%d\n",guarded);fclose(result);}
    return probe_end(guarded);
}

static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model;
    if(!ProMdlCurrentGet(&model))return probe_end(0);
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0)return probe_end(0);
    CloseHandle(native_ui_ready);
    native_ui_ready=NULL;
    /* Synthetic prior design, including the old misleading read-only answer. */
    wcscpy_s(conversation,_countof(conversation),
        L"用户：水杯先用无把手直筒方案，外径80mm、总高100mm、壁厚3mm、底厚5mm。\n"
        L"助手：记录方案。当前只分析，尚未新建。\n");
    native_mode=0;
    probe_started=GetTickCount64();
    if(!SetTimer(native_backend_window,5001,1000,probe_timer))return probe_end(0);
    native_prompt_set(probe_request);
    run_local(NULL,NULL,NULL);
    probe_write_allowed=smart_active&&!smart_readonly&&!request_requires_current;
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error;
    uiCmdCmdId command;
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,16)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    error=ProCmdActionAdd("BWTLModelRequestBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("BWTLModelRequestTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    return ProMacroLoad(L"~ Command `BWTLModelRequestBegin`;");
}
