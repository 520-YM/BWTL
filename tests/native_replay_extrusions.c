#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

typedef struct {ProDimension item;double value;int count;} ExtrusionDimension;

static ProError extrusion_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    ExtrusionDimension *found=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==PRODIMTYPE_LINEAR&&fabs(value-found->value)<1e-7){
        found->item=*dimension;
        found->count++;
    }
    return error;
}

static ProError extrusion_verify_edit(ProMdl model,const wchar_t *name,double from,double to,double change,FILE *log)
{
    ProFeature feature;
    ExtrusionDimension dimension={0};
    dimension.value=from;
    ProMassProperty before={0},after={0},restored={0};
    ProError error=ux_named_feature(model,name,&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,extrusion_dimension,NULL,&dimension);
    if(!error&&dimension.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error)error=ProDimensionValueSet(&dimension.item,to);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    if(!error&&fabs(after.volume-before.volume-change)>0.05)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&dimension.item,from);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&restored);
    if(!error&&fabs(restored.volume-before.volume)>0.05)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"EXTRUSION DIMENSION EDIT %ls ERROR=%d COUNT=%d DELTA=%.8f EXPECTED=%.8f\n",
        name,error,dimension.count,after.volume-before.volume,change);
    return error;
}

static ProError extrusion_trial(FILE *log,int separate)
{
    const wchar_t *solid_steps[]={
        L"DATUM_PLANE %ls ADD_Z PLANE:FRONT 20",
        L"SKETCH_CREATE %ls ADD_SK PLANE:ADD_Z PLANE:RIGHT RECT 10 10 12 8",
        L"SOLID_EXTRUDE_TWO %ls TWO_ADD ADD_SK ADD 7 3 POS",
        L"DATUM_PLANE %ls MID_Z PLANE:FRONT 10",
        L"SKETCH_CREATE %ls CUT_SK PLANE:MID_Z PLANE:RIGHT CIRCLE 40 30 3",
        L"SOLID_EXTRUDE_SYM %ls SYM_CUT CUT_SK CUT 6"
    };
    const wchar_t *body_steps[]={
        L"SKETCH_CREATE %ls BODY_SK PLANE:FRONT PLANE:RIGHT RECT 100 10 20 15",
        L"BODY_EXTRUDE %ls NEW_BODY BODY_SK 10 POS"
    };
    ProMdl source=NULL;
    ProMdlName name;
    Fixture stock;
    ProError error=fixture(&source,name,&stock);
    const wchar_t **steps=separate?body_steps:solid_steps;
    const int count=separate?_countof(body_steps):_countof(solid_steps);
    for(int i=0;!error&&i<count;i++){
        wchar_t formatted[512],command[512];
        swprintf_s(formatted,_countof(formatted),steps[i],name);
        swprintf_s(command,_countof(command),L"CREO_OP %ls",formatted);
        error=expansion_build(source,command,log);
    }
    if(!error&&separate){
        ProFeature feature;
        ProElement definition=NULL;
        error=ux_named_feature(source,L"NEW_BODY",&feature);
        if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&definition);
        if(!error)error=ProElemtreeWrite(definition,PRO_ELEMTREE_XML,L"separate-extrusion.xml");
        if(definition)ProFeatureElemtreeFree(&feature,definition);
    }
    fwprintf(log,L"EXTRUSION SOURCE SEPARATE=%d ERROR=%d\n",separate,error);
    if(!error)error=ProMdlSave(source);
    ProPath output=L"";
    wchar_t report[18000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&separate&&!wcsstr(report,L"拉伸新建实体"))error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"EXTRUSION REPLAY SEPARATE=%d ERROR=%d\n%ls\n",separate,error,report);
    ProMdl result=NULL;
    int bodies=0;
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    if(!error)error=sd_body_count(result,&bodies);
    if(!error&&bodies!=(separate?2:1))error=PRO_TK_INCOMPLETE;
    if(!error&&separate)error=extrusion_verify_edit(result,L"NEW_BODY",10,11,300,log);
    if(!error&&!separate)error=extrusion_verify_edit(result,L"TWO_ADD",3,4,96,log);
    if(!error&&!separate)error=extrusion_verify_edit(result,L"SYM_CUT",12,13,-9*AS_PI,log);
    if(result)ProMdlErase(result);
    return error;
}

static int extrusion_case(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-extrusions.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError same=extrusion_trial(log,0);
    ProError separate=extrusion_trial(log,1);
    fwprintf(log,L"SAME=%d SEPARATE=%d PASS=%d\n",same,separate,!same&&!separate);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLReplayExtrusions",extrusion_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayExtrusions\x0060;");
}
