#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES

static ProError remaining_variable(ProMdl model,const wchar_t *name,FILE *log){
    ProError error=batch_step(model,log,L"CREO_OP SKETCH_CREATE %ls PATH PLANE:FRONT PLANE:RIGHT LINE 0 0 30 0",name);
    if(!error)error=batch_step(model,log,L"CREO_OP SOLID_SWEEP %ls SWEEP CURVE:PATH 4",name);
    ProFeature sweep;
    ProElement tree=NULL,profile,mode;
    if(!error)error=ux_named_feature(model,L"SWEEP",&sweep);
    if(!error)error=ProFeatureElemtreeExtract(&sweep,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SWEEP_PROF_COMP},1,&profile);
    if(!error)error=mc_tree_element(profile,(ProElemId[]){PRO_E_SWP_SEC_TYPE},1,&mode);
    if(!error)error=ProElementIntegerSet(mode,PRO_SWEEP_VARIABLE_SECTION);
    if(!error)error=cm_apply_tree(&sweep,tree);
    if(tree)ProFeatureElemtreeFree(&sweep,tree);
    fwprintf(log,L"NATIVE VARIABLE SOURCE=%d\n",error);
    return error;
}

static ProError remaining_intersection(ProMdl source,const wchar_t *name,FILE *log){
        ProError error=batch_step(source,log,L"CREO_OP SKETCH_CREATE %ls STOCK_SK PLANE:FRONT PLANE:RIGHT RECT 1 1 35 20",name);
    if(!error)error=batch_step(source,log,L"CREO_OP BODY_EXTRUDE %ls STOCK STOCK_SK 2 POS",name);
    if(!error)error=batch_step(source,log,L"CREO_OP DATUM_PLANE %ls REF PLANE:RIGHT 30",name);
    if(!error)error=batch_step(source,log,L"CREO_OP DATUM_CURVE_INTERSECT %ls INTER PLANE:FRONT PLANE:REF",name);
    if(!error)error=batch_step(source,log,L"CREO_OP DATUM_POINT_CURVE %ls MARK CURVE:INTER PERCENT 40",name);
    return error;
}

static int remaining_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"replay-remaining.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    int pass=0;
    for(int i=0;i<2;i++){
        ProMdl source=NULL,result=NULL;
        ProMdlName name;
        ProError error=batch_empty(1,&source,name);
        if(!error)error=i==0?remaining_variable(source,name,log):remaining_intersection(source,name,log);
        if(!error)error=batch_reopen(source,&result,log);
        if(!error&&i==0)error=batch_edit(result,L"SWEEP",PRODIMTYPE_DIAMETER,4,5,log);
        if(result)ProMdlErase(result);
        if(source)ProMdlErase(source);
        fwprintf(log,L"CASE %d ERROR=%d\n",i,error);
        pass+=!error;
    }
    fwprintf(log,L"REMAINING CASES=%d PASS=%d\n",pass,pass==2);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayRemaining",remaining_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayRemaining\x0060;");
}

