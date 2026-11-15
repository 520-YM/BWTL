#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError boolean_source(ProMdl model,ProMdlName name,const wchar_t *operation,FILE *log)
{
    const wchar_t *steps[]={
        L"SKETCH_CREATE %ls BODY_A_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 10 12",
        L"BODY_EXTRUDE %ls BODY_A BODY_A_SK 10 POS",
        L"SKETCH_CREATE %ls BODY_B_SK PLANE:FRONT PLANE:RIGHT RECT 5 0 10 12",
        L"BODY_EXTRUDE %ls BODY_B BODY_B_SK 9 POS"
    };
    wchar_t command[512],step[500];
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<_countof(steps);i++){
        swprintf_s(step,_countof(step),steps[i],name);
        swprintf_s(command,_countof(command),L"CREO_OP %ls",step);
        error=expansion_build(model,command,log);
    }
    ProSolidBody *bodies=NULL;
    ProFeature a,b;
    int count=0,target=0,tool=0;
    if(!error)error=ux_named_feature(model,L"BODY_A",&a);
    if(!error)error=ux_named_feature(model,L"BODY_B",&b);
    if(!error)error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        int origin=0;
        error=replay_boolean_origin(&bodies[i],&origin);
        if(!error&&origin==a.id)target=bodies[i].id;
        if(!error&&origin==b.id)tool=bodies[i].id;
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(!error&&(!target||!tool))error=PRO_TK_E_NOT_FOUND;
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP BODY_BOOLEAN %ls BOOL_RESULT %ls BODY:%d BODY:%d",name,operation,target,tool);
        error=expansion_build(model,command,log);
    }
    return error;
}

typedef struct {ProDimension item;int count;} BooleanDepth;

static ProError boolean_depth(ProDimension *item,ProError status,ProAppData data)
{
    if(status)return status;
    BooleanDepth *found=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(item,&type);
    if(!error)error=ProDimensionValueGet(item,&value);
    if(!error&&type==PRODIMTYPE_LINEAR&&fabs(value-9)<1e-7){found->item=*item;found->count++;}
    return error;
}

static ProError boolean_verify(ProMdl model,int operation,double original,double change,FILE *log)
{
    ProFeature feature,body;
    ProFeattype type;
    ProElement tree=NULL;
    int actual=0;
    ProError error=ux_named_feature(model,L"BOOL_RESULT",&feature);
    if(!error)error=ProFeatureTypeGet(&feature,&type);
    if(!error&&type!=PRO_FEAT_BOOLEANBODIES)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error&&(!replay_boolean_definition(tree,&actual)||actual!=operation))error=PRO_TK_INCOMPLETE;
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    ProMassProperty before={0},after={0},restored={0};
    BooleanDepth depth={0};
    if(!error)error=ux_named_feature(model,L"BODY_B",&body);
    if(!error)error=ProFeatureDimensionVisit(&body,boolean_depth,NULL,&depth);
    if(!error&&depth.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error&&fabs(before.volume-original)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&depth.item,8);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    if(!error&&fabs(after.volume-before.volume-change)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&depth.item,9);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&restored);
    if(!error&&fabs(restored.volume-original)>0.01)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"BOOLEAN EDIT=%d KIND=%d DEPTH_COUNT=%d VOLUME=%.6f -> %.6f -> %.6f\n",
        error,actual,depth.count,before.volume,after.volume,restored.volume);
    return error;
}

static ProError boolean_trial(const wchar_t *operation,int kind,double expected,double change,FILE *log)
{
    ProMdl source=NULL,rebuilt=NULL;
    ProMdlName name;
    const char *stage="prepare body boolean";
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    if(!error)error=boolean_source(source,name,operation,log);
    fwprintf(log,L"BOOLEAN SOURCE %ls=%d\n",operation,error);
    if(!error)error=ProMdlSave(source);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&!wcsstr(report,L"多实体布尔"))error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"BOOLEAN REPLAY %ls=%d\n%ls\n",operation,error,report);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&rebuilt);
    if(!error)error=boolean_verify(rebuilt,kind,expected,change,log);
    return error;
}

static ProError split_source(ProMdl source)
{
    FdTxn tx={0};
    FdRef body={0},plane={0};
    ProElement branch=NULL;
    ProSolidBody *bodies=NULL;
    int count=0,active=0;
    ProError error=ProSolidBodiesCollect((ProSolid)source,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(!error&&state==PRO_BODY_STATE_ACTIVE){
            active++;
            body.item=bodies[i];
        }
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(!error&&active!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProSelectionAlloc(NULL,&body.item,&body.selection);
    trace("Split fixture target",error);
    if(!error)error=fd_ref_resolve(source,L"PLANE:SPLIT_AT",PRO_SURFACE,&plane);
    trace("Split fixture plane",error);
    if(!error)error=sh_begin(&tx,source,L"SPLIT_RESULT",PRO_FEAT_SPLIT_BODY);
    trace("Split fixture begin",error);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BY_OPTION,1,PRO_SPLIT_BY_SPLITTING_OBJ,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BODY_TYPE,1,PRO_SPLIT_BODY,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BODY,0,0,&branch);
    trace("Split fixture branch",error);
    if(!error)error=fd_ref_element(&tx,branch,PRO_E_SPLIT_TARGET_BODY,&body);
    if(!error)error=fd_ref_element(&tx,branch,PRO_E_SPLIT_BODY_REF,&plane);
    trace("Split fixture references",error);
    if(!error)error=model_element(branch,PRO_E_SPLIT_BODY_SLICE_OPT,1,PRO_E_SPLIT_BODY_SLICE_SECOND_OPT,NULL);
    if(!error)error=model_element(branch,PRO_E_SPLIT_TOOL_EXTEND_OPT,1,PRO_E_SPLIT_TOOL_EXTEND_OPT_NO,NULL);
    trace("Split fixture definition",error);
    if(!error)error=fd_create(&tx,0);
    trace("Split fixture create",error);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++){
        trace("Split fixture element",tx.errors.error_list[i].err_item_id);
        trace("Split fixture element status",tx.errors.error_list[i].error);
    }
    if(!error)error=part_regenerate(source);
    fd_ref_free(&plane);
    fd_ref_free(&body);
    fd_cleanup(&tx,error);
    return error;
}

static ProError split_read(ProMdl model,double expected_a,double expected_b)
{
    ProSolidBody *bodies=NULL;
    int count=0,active=0;
    double volumes[2]={0};
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        ProOutline bounds;
        if(active==2){error=PRO_TK_INCOMPLETE;break;}
        error=ProSolidBodyOutlineGet(&bodies[i],bounds);
        if(!error)volumes[active++]=(bounds[1][0]-bounds[0][0])*(bounds[1][1]-bounds[0][1])*(bounds[1][2]-bounds[0][2]);
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(!error&&(active!=2||fabs(fmin(volumes[0],volumes[1])-expected_a)>0.01||
       fabs(fmax(volumes[0],volumes[1])-expected_b)>0.01))error=PRO_TK_INCOMPLETE;
    return error;
}

static ProError split_trial(FILE *log)
{
    ProMdl source=NULL,rebuilt=NULL;
    ProMdlName name;
    Fixture stock;
    ProError error=fixture(&source,name,&stock);
    wchar_t command[512];
    swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls SPLIT_AT PLANE:FRONT 9",name);
    if(!error)error=expansion_build(source,command,log);
    if(!error)error=split_source(source);
    fwprintf(log,L"SPLIT CREATE=%d\n",error);
    if(!error)error=split_read(source,43200,52800);
    fwprintf(log,L"SPLIT SOURCE=%d\n",error);
    if(!error)error=ProMdlSave(source);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"SPLIT REPLAY=%d\n%ls\n",error,report);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&rebuilt);
    ProFeature split,plane;
    ProFeattype type;
    if(!error)error=ux_named_feature(rebuilt,L"SPLIT_RESULT",&split);
    if(!error)error=ProFeatureTypeGet(&split,&type);
    if(!error&&type!=PRO_FEAT_SPLIT_BODY)error=PRO_TK_INCOMPLETE;
    if(!error)error=split_read(rebuilt,43200,52800);
    if(!error)error=ux_named_feature(rebuilt,L"SPLIT_AT",&plane);
    BooleanDepth offset={0};
    if(!error)error=ProFeatureDimensionVisit(&plane,boolean_depth,NULL,&offset);
    if(!error&&offset.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&offset.item,8);
    if(!error)error=part_regenerate(rebuilt);
    if(!error)error=split_read(rebuilt,38400,57600);
    if(!error)error=ProDimensionValueSet(&offset.item,9);
    if(!error)error=part_regenerate(rebuilt);
    if(!error)error=split_read(rebuilt,43200,52800);
    fwprintf(log,L"SPLIT EDIT AND RESTORE=%d\n",error);
    return error;
}

static int boolean_test(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-boolean.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    ProError merge=boolean_trial(L"MERGE",PRO_MERGE_BOOL_TYPE,1740,-60,log);
    ProError subtract=boolean_trial(L"SUBTRACT",PRO_SUBTRACT_BOOL_TYPE,660,60,log);
    ProError intersect=boolean_trial(L"INTERSECT",PRO_INTERSECT_BOOL_TYPE,540,-60,log);
    ProError split=split_trial(log);
    fwprintf(log,L"MERGE=%d SUBTRACT=%d INTERSECT=%d SPLIT=%d PASS=%d\n",
        merge,subtract,intersect,split,!merge&&!subtract&&!intersect&&!split);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLReplayBoolean",boolean_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayBoolean\x0060;");
}
