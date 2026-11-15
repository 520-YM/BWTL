#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

typedef struct {ProDimension item;double original,new_value;int count;} SweepDiameter;

static ProError sweep_diameter_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    SweepDiameter *found=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&((type==PRODIMTYPE_DIAMETER&&fabs(value-4)<1e-7)||
       (type==PRODIMTYPE_RADIUS&&fabs(value-2)<1e-7))){
        found->item=*dimension;
        found->original=value;
        found->new_value=value*1.125;
        found->count++;
    }
    return error;
}

static ProError sweep_verify_edit(const wchar_t *output,int cut,FILE *log)
{
    ProMdl model=NULL;
    ProFeature feature;
    ProElement tree=NULL,sketch=NULL;
    ProSection section=NULL;
    SweepDiameter diameter={0};
    ProMassProperty before={0},after={0},restored={0};
    ProError error=ProMdlFiletypeLoad((wchar_t *)output,PRO_MDLFILE_PART,PRO_B_FALSE,&model);
    if(!error)error=ux_named_feature(model,L"CONST_SWEEP",&feature);
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=replay_sweep_section(tree,&sketch,&section);
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    if(!error)error=ProFeatureDimensionVisit(&feature,sweep_diameter_dimension,NULL,&diameter);
    if(!error&&diameter.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error)error=ProDimensionValueSet(&diameter.item,diameter.new_value);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    const double path=cut?80:30+15*AS_PI;
    const double expected=path*AS_PI*(2.25*2.25-2*2)*(cut?-1:1);
    if(!error&&fabs(after.volume-before.volume-expected)>0.05)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&diameter.item,diameter.original);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&restored);
    if(!error&&fabs(before.volume-restored.volume)>0.05)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SWEEP PROFILE EDIT CUT=%d ERROR=%d DIMENSIONS=%d VOLUME=%.8f/%.8f/%.8f\n",
        cut,error,diameter.count,before.volume,after.volume,restored.volume);
    if(model)ProMdlErase(model);
    return error;
}

static ProError constant_sweep_trial(FILE *log,int cut)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture stock;
    const char *stage="start sweep fixture";
    ProError error=cut?fixture(&source,name,&stock):model_create_empty(1,name,&stage);
    if(!error&&!cut)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    wchar_t command[512];
    if(!error&&cut){
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls CUT_LEVEL PLANE:FRONT 10",name);
        error=expansion_build(source,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),cut?
            L"CREO_OP SKETCH_CREATE %ls SWEEP_PATH PLANE:CUT_LEVEL PLANE:RIGHT LINE -5 25 85 25":
            L"CREO_OP SKETCH_CREATE %ls SWEEP_PATH PLANE:FRONT PLANE:RIGHT ARC 0 0 30 0 90",name);
        error=expansion_build(source,command,log);
    }
    if(!error&&!cut){
        swprintf_s(command,_countof(command),L"CREO_OP SKETCH_ADD %ls SWEEP_PATH LINE 0 30 -30 30",name);
        error=expansion_build(source,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),cut?
            L"CREO_OP SOLID_SWEEP_CUT %ls CONST_SWEEP CURVE:SWEEP_PATH CIRCLE 4":
            L"CREO_OP SOLID_SWEEP_CHAIN %ls CONST_SWEEP CURVE:SWEEP_PATH 4",name);
        error=expansion_build(source,command,log);
    }
    ProFeature source_sweep;
    ProElement native=NULL;
    if(!error)error=ux_named_feature(source,L"CONST_SWEEP",&source_sweep);
    if(!error)error=ProFeatureElemtreeExtract(&source_sweep,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&native);
    if(!error)error=ProElemtreeWrite(native,PRO_ELEMTREE_XML,cut?L"sweep-cut.xml":L"sweep-add.xml");
    if(native)ProFeatureElemtreeFree(&source_sweep,native);
    fwprintf(log,L"SWEEP SOURCE CUT=%d ERROR=%d\n",cut,error);
    if(!error)error=ProMdlSave(source);
    ProPath output=L"";
    wchar_t report[16000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"SWEEP REPLAY CUT=%d ERROR=%d\n%ls\n",cut,error,report);
    if(!error)error=sweep_verify_edit(output,cut,log);
    return error;
}

static int constant_sweep_case(uiCmdCmdId command,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-constant-sweep.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 1;}
    model_background_mode=1;
    ProError add=constant_sweep_trial(log,0);
    ProError cut=constant_sweep_trial(log,1);
    fwprintf(log,L"ADD=%d CUT=%d PASS=%d\n",add,cut,!add&&!cut);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLConstantSweep",constant_sweep_case,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLConstantSweep\x0060;");
}
