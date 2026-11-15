#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError definition_check(ProMdl source,int frozen,FILE *log)
{
    ProWVerstamp before=NULL,after=NULL;
    ProError error=ProMdlVerstampGet(source,&before);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_diagnose(source,&out);
    if(!error)error=ProMdlVerstampGet(source,&after);
    if(!error&&!ProVerstampEqual(before,after))error=PRO_TK_INCOMPLETE;
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    const wchar_t *definitions=wcsstr(report,L"源特征定义参照");
    if(!error&&(!definitions||!wcsstr(definitions,L"定义参照 ")))error=PRO_TK_INCOMPLETE;
    if(!error&&frozen&&(!wcsstr(report,L"冻结")||!wcsstr(definitions,L"定义 ")))error=PRO_TK_INCOMPLETE;
    if(!error&&!frozen&&(!wcsstr(report,L"子特征")||!wcsstr(report,L"父特征")||
                         !wcsstr(definitions,L"TEST_OFFSET")||!wcsstr(definitions,L"TEST_HOLE")||
                         !wcsstr(definitions,L"上游")||!wcsstr(definitions,L"（类型 ")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"REFERENCE REPORT FROZEN=%d ERROR=%d\n%ls\n",frozen,error,report);
    return error;
}

static ProError definition_active(FILE *log)
{
    ProMdl model=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&model,name,&geometry);
    wchar_t protocol[512];
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls TEST_OFFSET SURFACE:%d 12",name,geometry.top);
        error=expansion_build(model,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls TEST_HOLE SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(model,protocol,log);
    }
    return error?error:definition_check(model,0,log);
}

static ProError definition_frozen(FILE *log)
{
    ProPath path;
    ProMdl source=NULL;
    ProError error=GetEnvironmentVariableW(L"BWTL_PREVIEW_FROZEN",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_BAD_INPUTS;
    return error?error:definition_check(source,1,log);
}

static int definition_refs_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-definition-refs.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError active=definition_active(log);
    const ProError frozen=definition_frozen(log);
    fwprintf(log,L"ACTIVE=%d FROZEN=%d PASS=%d\n",active,frozen,!active&&!frozen);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayDefinitionRefs",definition_refs_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayDefinitionRefs\x0060;");
}
