#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError local_group_trial(FILE *log)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    wchar_t protocol[512],report[20000]=L"";
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls GROUP_X PLANE:RIGHT 34",name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP REUSE_GROUP %ls GROUP_SAMPLE GROUP_X SEED",name);
        error=expansion_build(source,protocol,log);
    }
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls FOLLOW PLANE:GROUP_X 13",name);
        error=expansion_build(source,protocol,log);
    }
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&(!wcsstr(report,L"本地特征组 GROUP_SAMPLE")||!wcsstr(report,L"组成员")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"GROUP PREVIEW=%d\n%ls\n",error,report);
    out=(PartText){report,_countof(report),0};
    report[0]=0;
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    if(!error&&(!wcsstr(report,L"原组 GROUP_SAMPLE")||!wcsstr(report,L"按原顺序核对 2 个成员")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"GROUP REPLAY=%d\n%ls\n",error,report);
    ProMdl result=NULL;
    ProFeature header,plane,seed,child;
    if(!error)error=ProMdlFiletypeLoad(delivered,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=ux_named_feature(result,L"GROUP_SAMPLE",&header);
    if(!error)error=ux_named_feature(result,L"GROUP_X",&plane);
    if(!error)error=ux_named_feature(result,L"SEED",&seed);
    if(!error)error=ux_named_feature(result,L"FOLLOW",&child);
    ReplayLocalGroup read={0};
    if(!error)error=replay_local_group_read(&header,&read);
    if(!error&&(read.count!=2||read.members[0]!=plane.id||read.members[1]!=seed.id))error=PRO_TK_INCOMPLETE;
    int *parents=NULL,count=0,found=0;
    if(!error)error=ProFeatureParentsGet(&child,&parents,&count);
    for(int i=0;!error&&i<count;i++)if(parents[i]==plane.id)found=1;
    if(!error&&!found)error=PRO_TK_INCOMPLETE;
    if(parents)ProArrayFree((ProArray *)&parents);
    fwprintf(log,L"GROUP ORDER AND CHILD=%d\n",error);
    if(!error)error=expansion_check_holes(delivered,1,20,log);
    return error;
}

static int local_group_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-local-group.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=local_group_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayLocalGroup",local_group_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayLocalGroup\x0060;");
}
