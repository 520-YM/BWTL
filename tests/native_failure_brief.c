#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl model;
static ProMdlName name;
static ProError setup_error;

static int run_failure(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    wchar_t protocol[420];
    ProError error=setup_error;
    if(!error&&!native_ui_open_threaded(NULL))error=PRO_TK_GENERAL_ERROR;
    int passed=0;
    if(!error){
        smart_begin(L"在当前钣金零件上画一个矩形草绘");
        smart_intent=SMART_INTENT_CURRENT;
        smart_readonly=0;
        smart_local_plan=1;
        error=part_snapshot_take_mode(model,&request_snapshot,0);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP SMT_SKETCH %ls TestBase PLANE:NO_SUCH_DATUM PLANE:RIGHT RECT -100 -75 200 150",name);
        if(part_command_parse(protocol,&pending_part)==1){
            execute_part_command();
            const wchar_t *card=wcsstr(history,L"【AI】");
            const wchar_t *brief=card?wcsstr(card,L"简单说："):NULL;
            passed=!smart_active&&smart_total_failures==1&&smart_steps==1&&
                brief&&!wcsstr(card+4,L"【AI】")&&!wcsstr(brief+1,L"简单说：")&&
                wcsstr(brief,L"画钣金草绘没成功")&&wcsstr(brief,L"指定的面和方向")&&
                wcsstr(smart_details,L"fd_sketch_setup(")&&!wcsstr(smart_details,L"简单说：")&&
                wcsstr(conversation,smart_last_failure_brief)!=NULL;
        }
    }
    FILE *log=fopen("native-failure-brief.log","w");
    if(log){
        fprintf(log,"NATIVE_FAILURE_BRIEF_PASS=%d\nNATIVE_FAILURE_BRIEF_ERROR=%d\n",passed,error);
        fclose(log);
    }
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int prepare_failure(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    const char *stage="";
    model_background_mode=0;
    setup_error=model_create_empty(2,name,&stage);
    if(!setup_error)setup_error=ProMdlnameInit(name,PRO_MDL_PART,&model);
    return ProMacroLoad(L"~ Command \x0060" L"BWTLRunFailureBrief\x0060;");
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLTestFailureBrief",prepare_failure,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLRunFailureBrief",run_failure,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLTestFailureBrief\x0060;");
}
