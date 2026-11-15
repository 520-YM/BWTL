#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError tree_structure_trial(FILE *log)
{
    ProMdl model=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&model,name,&faces);
    wchar_t command[512];
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls GROUP_X PLANE:RIGHT 34",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls GROUP_Y PLANE:TOP 13",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP REUSE_GROUP %ls SAMPLE_GROUP GROUP_X GROUP_Y",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 20 SURFACE:%d 30 6 THROUGH 0",
            name,faces.top,faces.x,faces.y);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP FEATURE_PATTERN_LINEAR %ls LINEAR SEED PLANE:RIGHT 3 15 POS",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls FOLLOW PLANE:GROUP_X 5",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_CREATE %ls PROFILE PLANE:FRONT PLANE:RIGHT CIRCLE 10 5 3",name);
        error=expansion_build(model,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_ADD %ls PROFILE CENTERLINE 0 -10 0 10",name);
        error=expansion_build(model,command,log);
    }
    ProFeature group,first,second,seed;
    if(!error)error=ux_named_feature(model,L"SAMPLE_GROUP",&group);
    if(!error)error=ux_named_feature(model,L"GROUP_X",&first);
    if(!error)error=ux_named_feature(model,L"GROUP_Y",&second);
    if(!error)error=ux_named_feature(model,L"SEED",&seed);
    if(!error)error=ProModelitemHide(&first);
    ProPattern pattern;
    ProFeature *members=NULL;
    int count=0;
    if(!error)error=ProFeaturePatternGet(&seed,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternMembersGet(&pattern,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    if(!error&&count!=3)error=PRO_TK_INCOMPLETE;
    ProWVerstamp before=NULL,after=NULL;
    if(!error)error=ProMdlVerstampGet(model,&before);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_diagnose(model,&out);
    if(!error)error=ProMdlVerstampGet(model,&after);
    if(!error&&!ProVerstampEqual(before,after))error=PRO_TK_INCOMPLETE;
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    fwprintf(log,L"DIAGNOSTIC UNCHANGED=%d\n%ls\n",error,report);
    if(!error){
        swprintf_s(command,_countof(command),L"组 ID %d 的成员顺序 GROUP_X ID %d、GROUP_Y ID %d",
            group.id,first.id,second.id);
        if(!wcsstr(report,command)||!wcsstr(report,L"所属组 ID")||!wcsstr(report,L"的截面 1")||
           !wcsstr(report,L"模板基准 RIGHT")||!wcsstr(report,L"位于页脚"))
            error=PRO_TK_INCOMPLETE;
    }
    fwprintf(log,L"GROUP ORDER AND SECTION OWNER=%d\n",error);
    if(!error){
        swprintf_s(command,_countof(command),L"种子 ID %d，从属实例 ",seed.id);
        const wchar_t *line=wcsstr(report,command);
        const wchar_t *end=line?wcschr(line,L'\n'):NULL;
        if(!line||!end)error=PRO_TK_INCOMPLETE;
        for(int i=0;!error&&i<count;i++){
            if(members[i].id==seed.id)continue;
            swprintf_s(command,_countof(command),L"ID %d",members[i].id);
            const wchar_t *found=wcsstr(line,command);
            if(!found||found>=end)error=PRO_TK_INCOMPLETE;
        }
    }
    if(members)ProArrayFree((ProArray *)&members);
    fwprintf(log,L"PATTERN SEED AND MEMBERS=%d\n",error);
    if(!error&&(!wcsstr(report,L"已隐藏，仍有 1 个下游子特征")||!wcsstr(report,L"构造图元 1 个")))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"HIDDEN DRIVER AND CONSTRUCTION=%d\n",error);
    return error;
}

static int tree_structure_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-tree-structure.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=tree_structure_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayTreeStructure",tree_structure_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayTreeStructure\x0060;");
}
