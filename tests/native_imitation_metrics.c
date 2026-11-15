#define user_initialize imitation_metrics_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProError metrics_box(double length,ProMdl *model){
    ModelPlan plan={0,length,80,20,0,1};
    ProMdlName name;
    const char *stage=NULL;
    model_background_mode=1;
    ProError error=model_create(&plan,name,&stage);
    return error?error:ProMdlnameInit(name,PRO_MDL_PART,model);
}

static int metrics_case(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"imitation-metrics.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProMdl source=NULL,result=NULL;
    ProWVerstamp source_before=NULL,source_after=NULL,result_before=NULL,result_after=NULL;
    wchar_t text[10000]=L"";
    PartText out={text,_countof(text),0};
    ProError error=metrics_box(100,&source);
    if(!error)error=metrics_box(102,&result);
    if(!error)error=ProMdlVerstampGet(source,&source_before);
    if(!error)error=ProMdlVerstampGet(result,&result_before);
    if(!error&&imitation_compare(source,result,&out)!=PRO_TK_INCOMPLETE)error=PRO_TK_INCOMPLETE;
    if(!error&&(!wcsstr(text,L"实体 1 → 1（差 +0），可量测面组 0 → 0（差 +0）")||
       !wcsstr(text,L"包络 100 → 102 mm（尺寸差 +2")||
       !wcsstr(text,L"实体表面积 23200 → 23600 mm²（差 +400）")||
       !wcsstr(text,L"实体体积 160000 → 163200 mm³（差 +3200）")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"MEASURED DIFFERENCE=%d\n%ls\n",error,text);
    text[0]=0;
    out=(PartText){text,_countof(text),0};
    if(!error)error=imitation_compare(source,source,&out);
    if(!error&&(!wcsstr(text,L"尺寸差 +0")||!wcsstr(text,L"实体体积 160000 → 160000 mm³（差 +0）")))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"MATCHING MODELS=%d\n%ls\n",error,text);
    if(!error)error=ProMdlVerstampGet(source,&source_after);
    if(!error)error=ProMdlVerstampGet(result,&result_after);
    if(!error&&(!ProVerstampEqual(source_before,source_after)||!ProVerstampEqual(result_before,result_after)))
        error=PRO_TK_INCOMPLETE;
    if(source_before)ProVerstampFree(&source_before);
    if(source_after)ProVerstampFree(&source_after);
    if(result_before)ProVerstampFree(&result_before);
    if(result_after)ProVerstampFree(&result_after);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=imitation_metrics_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLImitationMetrics",metrics_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLImitationMetrics\x0060;");
}

