#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl risk_model;

static int risk_finish(int pass){
    trace("TRACEBACK_RISK_TEST_PASS",pass);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int risk_verify(uiCmdCmdId id,uiCmdValue *value,void *context){
    for(size_t i=0;i<_countof(bwtl_traceback_risks);i++){
        PartCommand cmd={0};
        cmd.verb=PART_FOUNDATION;
        cmd.count=7;
        wcscpy_s(cmd.arg[0],PART_TOKEN_MAX,bwtl_traceback_risks[i].verb);
        wcscpy_s(cmd.arg[5],PART_TOKEN_MAX,L"NEXT");
        if(bwtl_traceback_risk(&cmd)!=&bwtl_traceback_risks[i])return risk_finish(0);
    }
    if(_countof(bwtl_traceback_risks)!=12)return risk_finish(0);
    PartCommand harmless={0};
    harmless.verb=PART_FOUNDATION;
    harmless.count=7;
    wcscpy_s(harmless.arg[0],PART_TOKEN_MAX,L"SOLID_EXTRUDE_TO");
    wcscpy_s(harmless.arg[5],PART_TOKEN_MAX,L"PLANE:FRONT");
    if(bwtl_traceback_risk(&harmless))return risk_finish(0);
    wcscpy_s(harmless.arg[0],PART_TOKEN_MAX,L"SMT_FORM");
    if(bwtl_traceback_risk(&harmless))return risk_finish(0);

    ProMdl current=NULL;
    if(ProMdlCurrentGet(&current)||current!=risk_model||!native_ui_open_threaded(NULL))return risk_finish(0);
    smart_begin(L"请执行当前零件的边缘尖角检查。");
    smart_intent=SMART_INTENT_READ;
    smart_readonly=1;
    if(part_snapshot_take(current,&request_snapshot))return risk_finish(0);
    int edge=0;
    for(int i=0;i<request_snapshot.edges;i++){
        if(request_snapshot.edge[i].type==PRO_ENT_LINE){edge=request_snapshot.edge[i].id;break;}
    }
    if(!edge)return risk_finish(0);
    wchar_t request[256];
    swprintf_s(request,_countof(request),L"CREO_OP CHK_SHARP %ls 95 EDGE:%d",request_snapshot.name,edge);
    if(part_command_parse(request,&pending_part)!=1)return risk_finish(0);
    execute_part_command();
    const wchar_t *warning=wcsstr(history,L"【回溯风险重点提醒】第 208 项");
    const wchar_t *reply=wcsstr(history,L"【AI】");
    return risk_finish(warning&&reply&&warning<reply&&wcsstr(reply,L"已核对 1 条")&&smart_steps==1);
}

static int risk_setup(uiCmdCmdId id,uiCmdValue *value,void *context){
    ModelPlan box={0,40,30,10,0,1};
    ProMdlName name;
    const char *stage="traceback notice test part";
    model_background_mode=0;
    ProError error=model_create(&box,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&risk_model);
    if(!error)error=dv_display_new(risk_model);
    if(error)return risk_finish(0);
    return ProMacroLoad(L"~ Command \140BWTLRiskVerify\140;");
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLRiskSetup",risk_setup,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLRiskVerify",risk_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLRiskSetup\140;");
}

