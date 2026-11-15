#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError coverage_trial(FILE *log)
{
    wchar_t label[64]=L"";
    HMENU menu=native_imitation_menu_create();
    if(!menu)return PRO_TK_CANT_OPEN;
    const int length=GetMenuStringW(menu,IMITATION_COVERAGE,label,_countof(label),MF_BYCOMMAND);
    DestroyMenu(menu);
    if(!length||wcscmp(label,L"已验收范围"))return PRO_TK_E_NOT_FOUND;
    fwprintf(log,L"MENU ENTRY=1\n");

    ProMdl before=NULL,after=NULL;
    const ProError current=ProMdlCurrentGet(&before);
    const size_t prior_length=wcslen(conversation);
    imitation_pending=IMITATION_COVERAGE;
    imitation_ui_command(0,NULL,NULL);
    if(current!=ProMdlCurrentGet(&after)||before!=after||
       prior_length!=wcslen(conversation)||imitation_pending||native_busy)return PRO_TK_INCOMPLETE;
    fwprintf(log,L"MODEL AND CONTEXT UNCHANGED=1\n");
    if(!wcsstr(history,L"独立 Creo 10 已验收的样件与范围")||
       !wcsstr(history,L"M26Z0181011-V01")||!wcsstr(history,L"10 个阵列孔联动")||
       !wcsstr(history,L"需单独验收")||!wcsstr(history,L"诊断当前模型与参照"))
        return PRO_TK_INCOMPLETE;
    fwprintf(log,L"REPORT AND LIMITS=1\n");
    return PRO_TK_NO_ERROR;
}

static int coverage_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-coverage.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    const ProError error=coverage_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayCoverage",coverage_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayCoverage\x0060;");
}
