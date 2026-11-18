#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError inferred_trial(FILE *log,ProIntf3DExportType format,int holes)
{
    ProMdl source=NULL,neutral=NULL,result=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error&&holes)error=batch_step(source,log,L"CREO_OP HOLE %ls DRILL SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 6 THROUGH 0",
        name,faces.top,faces.x,faces.y);
    ProPath directory,leaf,input=L"",delivered=L"";
    if(!error)error=ProDirectoryCurrentGet(directory);
    if(!error)error=dv_export_file(source,directory,format,leaf);
    if(!error&&swprintf_s(input,_countof(input),L"%ls\\%ls",directory,leaf)<0)error=PRO_TK_BAD_INPUTS;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_neutral_load(input,&neutral,&out);
    fwprintf(log,L"FORMAT=%d IMPORT=%d\n",format,error);
    if(!error)error=imitation_run_mode(neutral,delivered,0,1,&out);
    if(!error&&(!wcsstr(report,L"几何推断")||!delivered[0]))error=PRO_TK_INCOMPLETE;
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error&&holes)
        error=batch_edit(result,L"INFERRED_PROFILE",PRODIMTYPE_DIAMETER,6,6.5,log);
    if(result)ProMdlErase(result);
    if(neutral)ProMdlErase(neutral);
    if(source)ProMdlErase(source);
    fwprintf(log,L"FORMAT=%d REBUILD=%d\n%ls\n",format,error,report);
    return error;
}

static int inferred_case(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-inferred.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    HMENU menu=native_imitation_menu_create();
    const int menu_ok=menu&&GetMenuState(GetSubMenu(menu,3),IMITATION_NEUTRAL,MF_BYCOMMAND)!=(UINT)-1;
    if(menu)DestroyMenu(menu);
    fwprintf(log,L"NEUTRAL MENU=%d\n",menu_ok);
    const ProError step=inferred_trial(log,PRO_INTF_EXPORT_STEP,1);
    const ProError iges=inferred_trial(log,PRO_INTF_EXPORT_IGES,0);
    fwprintf(log,L"STEP=%d IGES=%d PASS=%d\n",step,iges,!step&&!iges&&menu_ok);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayInferred",inferred_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLReplayInferred`;");
}
