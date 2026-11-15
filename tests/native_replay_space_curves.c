#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError space_equation_source(ProMdl model,const wchar_t *label)
{
    FdTxn tx={0};
    FdRef reference={0};
    ProElement equation=NULL;
    ProError error=sh_begin(&tx,model,label,PRO_FEAT_CURVE);
    if(!error)error=fd_ref_resolve(model,L"CSYS:SPACE_FRAME",PRO_CSYS,&reference);
    if(!error)error=model_element(tx.root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_FROM_EQUATION,NULL);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_CRV_FR_EQ_REF_CSYS,&reference);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_CSYS_TYPE,1,PRO_CRV_FR_EQ_CSYS_TYPE_CARTESIAN,NULL);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MIN,2,0,NULL);
    if(!error)error=model_element(tx.root,PRO_E_CRV_FR_EQ_PARAM_MAX,2,1,NULL);
    if(!error)error=model_element(tx.root,PRO_E_CRV_ENTER_EQUATION,0,0,&equation);
    if(!error)error=ProElementWstringSet(equation,L"x = 12 + 30*t\ny = 14 + 8*t*t\nz = 25 + 9*t*t*t\n");
    if(!error)error=fd_create(&tx,0);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++){
        trace("equation curve element",tx.errors.error_list[i].err_item_id);
        trace("equation curve element error",tx.errors.error_list[i].error);
    }
    if(!error)error=part_regenerate(model);
    fd_ref_free(&reference);
    fd_cleanup(&tx,error);
    return error;
}

typedef struct {ProDimension item;int count;} SpaceCoordinate;

static ProError space_coordinate_dimension(ProDimension *dimension,ProError status,ProAppData data)
{
    if(status)return status;
    SpaceCoordinate *x=data;
    ProDimensiontype type;
    double value;
    ProError error=ProDimensionTypeGet(dimension,&type);
    if(!error)error=ProDimensionValueGet(dimension,&value);
    if(!error&&type==PRODIMTYPE_LINEAR&&fabs(value-10)<1e-7){
        x->item=*dimension;
        x->count++;
    }
    return error;
}

static ProError space_path_geometry(ProMdl model,ReplayCurve *samples,ProVector point)
{
    ProFeature path;
    ProGeomitem item;
    ProCurve curve;
    ProError error=ux_named_feature(model,L"SPACE_EQUATION",&path);
    if(!error)error=ad_single_curve(&path,&item,&curve);
    if(!error)error=replay_curve_read(&item,samples);
    FdRef marked={0};
    if(!error)error=fd_ref_resolve(model,L"POINT:SPACE_MARK",PRO_POINT,&marked);
    if(!error)error=ad_point(&marked,point);
    fd_ref_free(&marked);
    return error;
}

static ProError space_curve_edit(ProMdl model,FILE *log)
{
    ProFeature frame;
    SpaceCoordinate x={0};
    ReplayCurve before={0},edited={0},restored={0};
    ProVector start,moved,back;
    ProError error=ux_named_feature(model,L"SPACE_FRAME",&frame);
    if(!error)error=ProFeatureDimensionVisit(&frame,space_coordinate_dimension,NULL,&x);
    if(!error&&x.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=space_path_geometry(model,&before,start);
    if(!error)error=ProDimensionValueSet(&x.item,12.5);
    if(!error)error=part_regenerate(model);
    if(!error)error=space_path_geometry(model,&edited,moved);
    for(int i=0;!error&&i<_countof(edited.point);i++)edited.point[i][0]-=2.5;
    if(!error){
        moved[0]-=2.5;
        if(!replay_curve_same(&before,&edited)||!ad_near(start,moved,0.0002))error=PRO_TK_INCOMPLETE;
    }
    if(!error)error=ProDimensionValueSet(&x.item,10);
    if(!error)error=part_regenerate(model);
    if(!error)error=space_path_geometry(model,&restored,back);
    if(!error&&(!replay_curve_same(&before,&restored)||!ad_near(start,back,0.0002)))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SPACE CURVE AND PATH POINT EDIT=%d\n",error);
    return error;
}

static ProError space_curve_trial(FILE *log,int equation)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture stock;
    ProError error=fixture(&source,name,&stock);
    const wchar_t *label=equation?L"SPACE_EQUATION":L"SPACE_SPLINE";
    if(!error&&equation){
        wchar_t command[512];
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_CSYS %ls SPACE_FRAME CSYS:PRT_CSYS_DEF 10 20 30 0 0 0",name);
        error=expansion_build(source,command,log);
        if(!error)error=space_equation_source(source,label);
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_POINT_CURVE %ls SPACE_MARK CURVE:%ls PERCENT 40",name,label);
        if(!error)error=expansion_build(source,command,log);
    }
    if(!error&&!equation){
        wchar_t command[512];
        swprintf_s(command,_countof(command),L"CREO_OP DATUM_CURVE_SPLINE %ls %ls CSYS:PRT_CSYS_DEF 10 12 25 18 21 30 28 16 36 42 24 42",
            name,label);
        error=expansion_build(source,command,log);
    }
    fwprintf(log,L"CURVE SOURCE KIND=%d ERROR=%d\n",equation,error);
    ProFeature feature;
    ProGeomitem item;
    ProCurve curve;
    ReplayCurve original={0},result={0};
    if(!error)error=ux_named_feature(source,label,&feature);
    ProFeattype type;
    if(!error)error=ProFeatureTypeGet(&feature,&type);
    ProElement definition=NULL;
    const ProError extraction=error?error:ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&definition);
    int definition_type=-1;
    if(definition){
        replay_tree_integer(definition,PRO_E_CURVE_TYPE,&definition_type);
        ProFeatureElemtreeFree(&feature,definition);
    }
    fwprintf(log,L"CURVE TYPE KIND=%d FEATURE=%d DEFINITION=%d EXTRACT=%d\n",
        equation,error?0:type,definition_type,extraction);
    if(!error)error=ad_single_curve(&feature,&item,&curve);
    if(!error)error=replay_curve_read(&item,&original);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=ProMdlSave(source);
    if(!error)replay_preview(source,&out);
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"CURVE REPLAY KIND=%d ERROR=%d\n%ls\n",equation,error,report);
    if(!equation)return error==PRO_TK_UNSUPPORTED&&extraction==PRO_TK_INVALID_TYPE&&!output[0]?
        PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
    ProMdl rebuilt=NULL;
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&rebuilt);
    if(!error)error=ux_named_feature(rebuilt,label,&feature);
    if(!error)error=ad_single_curve(&feature,&item,&curve);
    if(!error)error=replay_curve_read(&item,&result);
    if(!error&&!replay_curve_same(&original,&result))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"CURVE SAVED GEOMETRY KIND=%d ERROR=%d\n",equation,error);
    if(!error)error=space_curve_edit(rebuilt,log);
    return error;
}

static int space_curves_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-space-curves.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError equation=space_curve_trial(log,1);
    const ProError spline=space_curve_trial(log,0);
    fwprintf(log,L"EQUATION=%d IMPORT_REJECTED=%d PASS=%d\n",equation,spline,!equation&&!spline);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplaySpaceCurves",space_curves_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplaySpaceCurves\x0060;");
}
