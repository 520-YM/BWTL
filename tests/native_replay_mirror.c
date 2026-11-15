#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError mirror_trial(FILE *log,int multiple)
{
    ProMdl model=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&model,name,&geometry);
    wchar_t protocol[512],report[16000]=L"";
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls MIRROR_X PLANE:RIGHT 40",name);
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        if(multiple){
            swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls OTHER SURFACE:%d SURFACE:%d 32 SURFACE:%d 35 5 THROUGH 0",
                name,geometry.top,geometry.x,geometry.y);
            error=expansion_build(model,protocol,log);
        }
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),multiple?
            L"CREO_OP REUSE_MIRROR %ls MIRRORED PLANE:MIRROR_X SEED OTHER":
            L"CREO_OP FEATURE_MIRROR %ls MIRRORED SEED PLANE:MIRROR_X",name);
        error=expansion_build(model,protocol,log);
    }
    PartText out={report,_countof(report),0};
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(model,delivered,0,1,&out);
    fwprintf(log,L"MIRROR MODE=%d REPLAY=%d\n%ls\n",multiple,error,report);
    fflush(log);
    if(!error&&(!wcsstr(report,L"特征几何镜像 MIRRORED")||!wcsstr(report,L"项组内派生特征")))error=PRO_TK_INCOMPLETE;
    if(!error)error=expansion_check_holes(delivered,2,20,log);
    return error;
}

static int mirror_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-mirror.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError single=mirror_trial(log,0);
    const ProError multiple=mirror_trial(log,1);
    fwprintf(log,L"SINGLE=%d MULTIPLE=%d PASS=%d\n",single,multiple,!single&&!multiple);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayMirror",mirror_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayMirror\x0060;");
}
