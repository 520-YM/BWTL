#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError size_axis(ProElement pattern,ProElemId field,ProFeature *seed,
                          double position,double pitch)
{
    ReplayDimensionMatch match={0};
    match.type=PRODIMTYPE_LINEAR;
    match.value=position;
    ProError error=ProFeatureDimensionVisit(seed,replay_match_dimension,NULL,&match);
    if(!error&&match.count!=1)error=PRO_TK_INCOMPLETE;
    ProElement axis=NULL,entry=NULL,selected=NULL,*entries=NULL;
    ProSelection selection=NULL;
    ProReference reference=NULL;
    if(!error)error=ProSelectionAlloc(NULL,&match.target,&selection);
    if(!error)error=ProSelectionToReference(selection,&reference);
    if(!error)error=model_element(pattern,field,0,0,&axis);
    if(!error)error=ProElementAlloc(PRO_E_GENPAT_DIM_DIR_COMPOUND,&entry);
    if(!error)error=model_element(entry,PRO_E_GENPAT_DIR_DIMENSION,0,0,&selected);
    if(!error)error=ProElementReferenceSet(selected,reference);
    if(!error)error=model_element(entry,PRO_E_GENPAT_DIR_VAR_TYPE,1,PRO_PAT_VALUE_DRIVEN,NULL);
    if(!error)error=model_element(entry,PRO_E_GENPAT_DIR_VAR_VALUE,2,pitch,NULL);
    if(!error)error=ProArrayAlloc(1,sizeof(*entries),1,(ProArray *)&entries);
    if(!error){
        entries[0]=entry;
        error=ProElementArraySet(axis,NULL,entries);
        if(!error)entry=NULL;
    }
    if(selection)ProSelectionFree(&selection);
    if(reference)ProReferenceFree(reference);
    if(entries)ProArrayFree((ProArray *)&entries);
    if(entry)ProElementFree(&entry);
    return error;
}

static ProError size_source(ProFeature *seed,int both)
{
    ProElement root=NULL,branch=NULL;
    ProError error=ProElementAlloc(PRO_E_PATTERN_ROOT,&root);
    if(!error)error=model_element(root,PRO_E_GENPAT_TYPE,1,PRO_GENPAT_DIM_DRIVEN,NULL);
    if(!error)error=model_element(root,PRO_E_GENPAT_REGEN_METHOD,1,PRO_PAT_GENERAL,NULL);
    if(!error)error=model_element(root,PRO_E_GENPAT_DIM,0,0,&branch);
    if(!error)error=size_axis(branch,PRO_E_GENPAT_DIM_FIRST_DIR,seed,25,15);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIM_FIRST_DIR_NUM_INST,1,3,NULL);
    if(!error&&both)error=size_axis(branch,PRO_E_GENPAT_DIM_SECOND_DIR,seed,20,12);
    if(!error&&!both)error=model_element(branch,PRO_E_GENPAT_DIM_SECOND_DIR,0,0,NULL);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIM_SECOND_DIR_NUM_INST,1,both?2:1,NULL);
    if(!error)error=ProPatternCreate(seed,PRO_FEAT_PATTERN,root);
    if(root)ProElementFree(&root);
    return error?error:part_regenerate(seed->owner);
}

static ProError size_direction_source(ProFeature *seed)
{
    FdTxn tx={0};
    FdRef x={0},y={0};
    ProError error=ProElementAlloc(PRO_E_PATTERN_ROOT,&tx.root);
    if(!error)error=fd_ref_resolve(seed->owner,L"PLANE:RIGHT",PRO_SURFACE,&x);
    if(!error)error=fd_ref_resolve(seed->owner,L"PLANE:TOP",PRO_SURFACE,&y);
    if(!error)error=rf_pattern_tree(&tx,&x,0,3,15,1);
    ProElement branch=NULL,second=NULL,direction=NULL;
    if(!error)error=mc_tree_element(tx.root,(ProElemId[]){PRO_E_GENPAT_DIR},1,&branch);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIR2,0,0,&second);
    if(!error)error=model_element(second,PRO_E_DIRECTION_COMPOUND,0,0,&direction);
    if(!error)error=fd_ref_element(&tx,direction,PRO_E_DIRECTION_REFERENCE,&y);
    if(!error)error=model_element(direction,PRO_E_DIRECTION_FLIP,1,PRO_DIRECTION_FLIP_ALONG,NULL);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIR2_INC,2,12,NULL);
    if(!error)error=model_element(branch,PRO_E_DIR_PAT_DIR2_FLIP,1,0,NULL);
    if(!error)error=model_element(branch,PRO_E_DIR_PAT_DIR2_OPT,1,PRO_GENPAT_TRANSLATIONAL,NULL);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIM_SECOND_DIR,0,0,NULL);
    if(!error)error=model_element(branch,PRO_E_GENPAT_DIM_SECOND_DIR_NUM_INST,1,2,NULL);
    if(!error)error=ProPatternCreate(seed,PRO_FEAT_PATTERN,tx.root);
    fd_ref_free(&x);
    fd_ref_free(&y);
    fd_cleanup(&tx,error);
    return error?error:part_regenerate(seed->owner);
}

static ProError size_pattern_case(FILE *log,int mode)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    wchar_t protocol[512],report[18000]=L"";
    PartText out={report,_countof(report),0};
    if(!error){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(source,protocol,log);
    }
    ProFeature seed;
    if(!error)error=ux_named_feature(source,L"SEED",&seed);
    if(!error&&mode<2)error=size_source(&seed,mode);
    if(!error&&mode==2){
        swprintf_s(protocol,_countof(protocol),L"CREO_OP FEATURE_PATTERN_LINEAR %ls DIR_PATTERN SEED PLANE:RIGHT 3 15 POS",name);
        error=expansion_build(source,protocol,log);
    }
    if(!error&&mode==3)error=size_direction_source(&seed);
    fwprintf(log,L"SETUP MODE=%d ERROR=%d\n",mode,error);
    fflush(log);
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    fwprintf(log,L"REPLAY MODE=%d ERROR=%d\n%ls\n",mode,error,report);
    fflush(log);
    if(!error)error=expansion_check_holes(delivered,mode%2?6:3,20,log);
    return error;
}

static int size_pattern_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-dimension-pattern.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError one=size_pattern_case(log,0);
    const ProError both=size_pattern_case(log,1);
    const ProError direction=size_pattern_case(log,2);
    const ProError direction_both=size_pattern_case(log,3);
    fwprintf(log,L"DIMENSION_ONE=%d DIMENSION_BOTH=%d DIRECTION=%d DIRECTION_BOTH=%d PASS=%d\n",
        one,both,direction,direction_both,!one&&!both&&!direction&&!direction_both);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayDimensionPattern",size_pattern_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayDimensionPattern\x0060;");
}
