#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError library_trial(FILE *log)
{
    ProPath profile;
    if(!GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile)))return PRO_TK_BAD_CONTEXT;
    ProPath expected;
    swprintf_s(expected,_countof(expected),L"%ls\\data",profile);
    if(_wcsicmp(runtime_data,expected))return PRO_TK_BAD_CONTEXT;
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    ProPath delivered=L"";
    wchar_t details[PART_TEXT_MAX]=L"";
    PartText report={details,_countof(details),0};
    ImitationOutcome outcome={0};
    if(!error)error=imitation_run_mode_impl(source,delivered,0,1,&outcome,&report);
    if(!error&&!outcome.edit_verified)error=PRO_TK_INCOMPLETE;
    if(!error&&!imitation_record(delivered,&imitation_last,source,&outcome,&report))error=PRO_TK_INCOMPLETE;
    details[0]=0;
    report=(PartText){details,_countof(details),0};
    const wchar_t *leaf=wcsrchr(delivered,L'\\');
    ProPath root,standards,library,archived,receipt;
    if(!error&&!leaf)error=PRO_TK_INCOMPLETE;
    if(!error){
        swprintf_s(root,_countof(root),L"%ls\\公司资料",profile);
        swprintf_s(standards,_countof(standards),L"%ls\\公司标准",root);
        swprintf_s(library,_countof(library),L"%ls\\公司标准件",standards);
        swprintf_s(archived,_countof(archived),L"%ls%ls",library,leaf);
        swprintf_s(receipt,_countof(receipt),L"%ls.验收.txt",archived);
        if(!imitation_library_folder(root)||!imitation_library_folder(standards)||
           !imitation_library_folder(library)||!CreateDirectoryW(archived,NULL))error=PRO_TK_CANT_WRITE;
        else{
            const ProError failure=imitation_save_library(&imitation_last,&report);
            if(failure!=PRO_TK_CANT_WRITE||GetFileAttributesW(receipt)!=INVALID_FILE_ATTRIBUTES)
                error=PRO_TK_INCOMPLETE;
            if(!RemoveDirectoryW(archived)&&!error)error=PRO_TK_CANT_WRITE;
        }
    }
    fwprintf(log,L"LIBRARY FAILED COPY CLEANUP=%d\n",error);
    details[0]=0;
    report=(PartText){details,_countof(details),0};
    if(!error)error=imitation_save_library(&imitation_last,&report);
    if(!error&&(!dv_file_nonempty(archived)||!dv_file_nonempty(receipt)))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"LIBRARY ARCHIVE=%d\n%ls\n",error,details);
    if(!error){
        FILE *file=_wfopen(receipt,L"rt, ccs=UTF-8");
        if(!file)error=PRO_TK_CANT_OPEN;
        else{
            wchar_t body[PART_TEXT_MAX]=L"";
            size_t size=fread(body,sizeof(wchar_t),_countof(body)-1,file);
            body[size]=0;
            fclose(file);
            if(!wcsstr(body,name)||!wcsstr(body,L"尺寸试改 已通过并恢复原值")||
               !wcsstr(body,L"第 1 步")||!wcsstr(body,L"临时改动尺寸 ID"))error=PRO_TK_INCOMPLETE;
        }
    }
    fwprintf(log,L"LIBRARY RECEIPT CONTENT=%d\n",error);
    if(!error&&imitation_save_library(&imitation_last,&report)!=PRO_TK_E_FOUND)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"LIBRARY DUPLICATE=%d\n",error);
    return error;
}

static int library_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-library.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=library_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayLibrary",library_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayLibrary\x0060;");
}
