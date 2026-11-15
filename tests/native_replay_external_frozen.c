#define user_initialize external_frozen_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int frozen_reference_case(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-external-frozen.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath path;
    ProMdl source=NULL;
    ProError error=GetEnvironmentVariableW(L"BWTL_PREVIEW_FROZEN",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_BAD_INPUTS;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    ProWVerstamp before=NULL,after=NULL;
    if(!error)error=ProMdlVerstampGet(source,&before);
    if(!error)imitation_reference_report(source,&out);
    if(!error)error=ProMdlVerstampGet(source,&after);
    if(!error&&(!ProVerstampEqual(before,after)||!wcsstr(report,L"当前无法直接绑定")))error=PRO_TK_INCOMPLETE;
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    fwprintf(log,L"%ls\nPASS=%d ERROR=%d\n",report,!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=external_frozen_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLFrozenReference",frozen_reference_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLFrozenReference`;");
}
