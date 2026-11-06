/* Exercise the installed chat path in a disposable Creo session. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl probe_model;
static ULONGLONG probe_started;
static int probe_sent,probe_queued;

static int probe_finish(int passed){
    FILE *f=fopen("revolve-cut-chat-result.log","a");
    if(f){fprintf(f,"REVOLVE_CUT_CHAT_%s\n",passed?"PASS":"FAIL");fclose(f);}
    KillTimer(native_backend_window,5001);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!probe_queued)probe_queued=ProMacroLoad(L"~ Command `BWTLRevolveCutProbeTick`;")==PRO_TK_NO_ERROR;
}

static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active;ProModelitem cut;
    ProFeattype type;
    ProMassProperty mass;
    const double pi=3.141592653589793,expected=9000*pi-60*pi*pi;
    const wchar_t *reply;
    int bodies;
    probe_queued=0;
    if(GetTickCount64()-probe_started>250000)return probe_finish(0);
    if(smart_active||ai_process)return 0;
    if(ProMdlCurrentGet(&active)||active!=probe_model)return probe_finish(0);
    if(!probe_sent){
        if(open_window(0,NULL,NULL))return probe_finish(0);
        probe_sent=1;
        native_prompt_set(L"在当前零件使用已有闭合草绘 CIRC_SK，绕基准轴 REV_AXIS 正向旋转切除180度，新特征命名 CIRC_CUT，不保存。");
        run_local(NULL,NULL,NULL);
        if(!smart_active||smart_readonly||!request_requires_current)return probe_finish(0);
        return 0;
    }
    reply=wcsstr(history,L"【AI】");
    if(!reply||wcsstr(reply+4,L"【AI】")||smart_steps<1||smart_failures||
       ProModelitemByNameInit(active,PRO_FEATURE,L"CIRC_CUT",&cut)||
       ProFeatureTypeGet((ProFeature *)&cut,&type)||type!=PRO_FEAT_CUT||
       part_health(active)||sd_body_count(active,&bodies)||bodies!=1||
       ProSolidMassPropertyWithDensityGet((ProSolid)active,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)||
       !part_close(mass.volume,expected))return probe_finish(0);
    {
        FILE *f=fopen("revolve-cut-chat-result.log","a");
        if(f){fprintf(f,"volume_mm3=%.7f expected_mm3=%.7f steps=%d single_reply=1\n",mass.volume,expected,smart_steps);fclose(f);}
    }
    return probe_finish(1);
}

static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    const wchar_t *verbs[]={L"DATUM_AXIS",L"SKETCH_CREATE",L"SOLID_REVOLVE",L"SKETCH_CREATE"};
    const wchar_t *tails[]={L"REV_AXIS PLANES PLANE:TOP PLANE:RIGHT",L"REV_SK PLANE:TOP PLANE:RIGHT RECT 10 0 10 30",
        L"REV_ADD REV_SK AXIS:REV_AXIS 360 POS",L"CIRC_SK PLANE:TOP PLANE:RIGHT CIRCLE 15 12 2"};
    wchar_t protocol[1024],report[PART_TEXT_MAX];ProMdlName name;
    const char *stage="isolated revolve fixture";
    PartCommand cmd;int i;
    if(model_create_empty(1,name,&stage)||ProMdlnameInit(name,PRO_MDL_PART,&probe_model))return probe_finish(0);
    for(i=0;i<_countof(verbs);i++){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP %ls %ls %ls",verbs[i],name,tails[i]);
        if(part_command_parse(protocol,&cmd)!=1||foundation_execute(&cmd,probe_model,report,_countof(report)))return probe_finish(0);
    }
    probe_started=GetTickCount64();
    return SetTimer(native_backend_window,5001,1000,probe_timer)?0:probe_finish(0);
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error;uiCmdCmdId command;char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,16)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    error=ProCmdActionAdd("BWTLRevolveCutProbeBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("BWTLRevolveCutProbeTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    return ProMacroLoad(L"~ Command `BWTLRevolveCutProbeBegin`;");
}
