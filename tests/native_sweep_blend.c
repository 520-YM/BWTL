#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
#include <ProUICmd.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int error){
    FILE *f=fopen("sweep-blend-native.log","a");
    if(f){fprintf(f,"%s: %d\n",label,error);fclose(f);}
}
#include "../src/native_model.h"
#include "../src/part_tools.h"

static ProMdlName fixture_name;

static ProError fixture_run(ProMdl model,const wchar_t *verb,const wchar_t *args){
    wchar_t text[1024],report[PART_TEXT_MAX]=L"";
    PartCommand cmd={0};
    ProError error;
    FILE *f;
    swprintf_s(text,_countof(text),L"CREO_OP %ls %ls %ls",verb,fixture_name,args);
    error=part_command_parse(text,&cmd)==1?foundation_execute(&cmd,model,report,_countof(report)):PRO_TK_BAD_INPUTS;
    f=_wfopen(L"sweep-blend-cases.log",L"a, ccs=UTF-8");
    if(f){fwprintf(f,L"%ls\nRESULT=%d\n%ls\n",text,error,report);fclose(f);}
    return error;
}

static ProError fixture_empty(ProMdl *model){
    const char *stage="new fixture";
    ProError e=model_create_empty(1,fixture_name,&stage);
    if(!e)e=ProMdlnameInit(fixture_name,PRO_MDL_PART,model);
    trace(stage,e);
    return e;
}

static int fixture_volume(ProMdl model,double expected){
    ProMassProperty mass;
    if(ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass))return 0;
    return part_close(mass.volume,expected);
}

static int fixture_reject(ProMdl model,const wchar_t *verb,const wchar_t *args,const wchar_t *name,double volume){
    FdTxn before={0},after={0};
    ProModelitem feature;
    int i;
    if(ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&before))return 0;
    if(fixture_run(model,verb,args)==PRO_TK_NO_ERROR||
       ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,&feature)!=PRO_TK_E_NOT_FOUND||
       ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&after)||
       before.before_count!=after.before_count||part_health(model)||!fixture_volume(model,volume))return 0;
    for(i=0;i<after.before_count;i++)if(!fd_was_feature(&before,after.before_ids[i]))return 0;
    return 1;
}

static int fixture_sweep(void){
    ProMdl model=NULL;
    double pi=3.141592653589793,volume=pi*4*pi*30/2;
    if(fixture_empty(&model)||
       fixture_run(model,L"SKETCH_CREATE",L"ARC_PATH PLANE:TOP PLANE:RIGHT ARC 0 0 30 0 90")||
       fixture_run(model,L"SKETCH_CREATE",L"CLOSED_PATH PLANE:TOP PLANE:RIGHT CIRCLE 0 0 15")||
       fixture_run(model,L"SOLID_SWEEP",L"SWEEP_OK CURVE:ARC_PATH 4")||
       !fixture_volume(model,volume))return 0;
    if(!fixture_reject(model,L"SOLID_SWEEP",L"BAD_SWEEP CURVE:CLOSED_PATH 4",L"BAD_SWEEP",volume))return 0;
    if(!fixture_reject(model,L"SOLID_SWEEP",L"OVERSIZE_SWEEP CURVE:ARC_PATH 100",L"OVERSIZE_SWEEP",volume))return 0;
    if(fixture_empty(&model)||
       fixture_run(model,L"SKETCH_CREATE",L"LINE_PATH PLANE:TOP PLANE:RIGHT LINE 0 0 30 0")||
       fixture_run(model,L"SOLID_SWEEP",L"STRAIGHT_SWEEP CURVE:LINE_PATH 4")||
       !fixture_volume(model,pi*4*30))return 0;
    return 1;
}

static int fixture_blend(void){
    ProMdl model=NULL;
    double volume=3.141592653589793*25*(13*13+13*7+7*7)/3;
    if(fixture_empty(&model)||
       fixture_run(model,L"DATUM_PLANE",L"END_PLANE PLANE:TOP 25")||
       fixture_run(model,L"SKETCH_CREATE",L"LOFT_START PLANE:TOP PLANE:RIGHT CIRCLE 0 0 13")||
       fixture_run(model,L"SKETCH_CREATE",L"LOFT_END PLANE:END_PLANE PLANE:RIGHT CIRCLE 0 0 7")||
       fixture_run(model,L"SKETCH_CREATE",L"COPLANAR PLANE:TOP PLANE:RIGHT CIRCLE 0 0 8")||
       fixture_run(model,L"SOLID_BLEND",L"LOFT_OK LOFT_START LOFT_END")||
       !fixture_volume(model,volume))return 0;
    return fixture_reject(model,L"SOLID_BLEND",L"BAD_LOFT LOFT_START COPLANAR",L"BAD_LOFT",volume)&&
        fixture_reject(model,L"SOLID_BLEND",L"DUPLICATE_LOFT LOFT_START LOFT_END",L"DUPLICATE_LOFT",volume);
}

static int fixture_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    int sweep=fixture_sweep(),blend=fixture_blend();
    trace("SWEEP_NATIVE_FAILURES",!sweep);
    trace("BLEND_NATIVE_FAILURES",!blend);
    trace("SWEEP_BLEND_NATIVE_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;
    ProError e;
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLFixtureStartProfile",fixture_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLFixtureStartProfile`;");
}

__declspec(dllexport) void user_terminate(void){}
