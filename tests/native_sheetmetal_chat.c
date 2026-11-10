#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int cases,failures;
static ULONGLONG started;
static ProMdl test_before;

static void check(int pass,const char *label){
    cases++;
    if(!pass)failures++;
    FILE *f=fopen("sheetmetal-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int finish(void){
    if(native_backend_window)KillTimer(native_backend_window,5419);
    smart_cancel();
    FILE *f=fopen("sheetmetal-chat.log","a");
    if(f){fprintf(f,"SMT_CHAT_CASES: %d\nSMT_CHAT_FAILURES: %d\nSMT_CHAT_DONE: 1\n",cases,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static int verification_tick(uiCmdCmdId command,uiCmdValue *value,void *data){

    if(smart_active||ai_process){
        if(GetTickCount64()-started<60000)return 0;
        check(0,"local model did not complete");return finish();
    }
    ProMdl model=NULL;
    ProMdlsubtype type;
    ProFeature feature;
    ProMassProperty material;
    int bodies=0;
    check(!ProMdlCurrentGet(&model)&&model&&model!=test_before&&!ProMdlSubtypeGet(model,&type)&&type==PROMDLSTYPE_PART_SHEETMETAL,
        "request creates separate actual sheetmetal model");
    check(model&&!part_health(model)&&!sd_body_count(model,&bodies)&&bodies==1&&!qa_geometry_mass(model,&material)&&material.volume>60000,
        "tray has healthy solid with added material");
    for(int i=1;i<=4;i++){
        ProName name;
        ProFeattype feature_type=0;
        swprintf_s(name,PRO_NAME_SIZE,L"DEMO_TRAY_W%d",i);
        check(model&&!ux_named_feature(model,name,&feature)&&!ProFeatureTypeGet(&feature,&feature_type)&&feature_type==PRO_FEAT_WALL,
            "native active flange in model tree");
    }
    check(!ai_process&&wcsstr(smart_details,L"CASE_TRAY ")&&wcsstr(smart_details,L"实体和重生成"),
        "native completion receipt without AI planning dependency");
    return finish();
}

static void CALLBACK tick(HWND window,UINT message,UINT_PTR timer,DWORD now){
    ProMacroLoad(L"~ Command `BWTLSheetmetalTick`;");
}

static int request(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(!native_ui_open_threaded(NULL)||!SetTimer(native_backend_window,5419,1000,tick)){
        check(0,"start private BWTL chat");return finish();
    }
    ProMdlCurrentGet(&test_before);
    started=GetTickCount64();
    native_prompt_set(L"验证一下之前的钣金件功能。给我建个模型。");
    run_local(NULL,NULL,NULL);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLSheetmetalRequest",request,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLSheetmetalTick",verification_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLSheetmetalRequest`;");
}
