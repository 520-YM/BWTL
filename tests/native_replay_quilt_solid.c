#define BWTL_REUSE_QUILT_EDITS
#include "native_replay_quilt_edits.c"
#undef BWTL_REUSE_QUILT_EDITS

static ProError quilt_cut_source(ProMdl model,int variant)
{
    FdTxn tx={0};
    FdRef reference={0};
    ProElement body=NULL;
    ProError error=sh_begin(&tx,model,L"QUILT_SOLID",PRO_FEAT_CUT);
    if(!error)error=model_element(tx.root,PRO_E_FEATURE_FORM,1,PRO_USE_SURFS,NULL);
    if(!error&&variant==4){
        error=sf_ref(model,L"QUILT:PATCH",&reference);
        if(!error)error=model_element(tx.root,PRO_E_FEAT_FORM_ALWAYS_THIN,1,PRO_THIN,NULL);
        if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_STD_USEQLT_QLT,&reference);
        if(!error)error=model_element(tx.root,PRO_E_STD_USEQLT_SIDE,1,PRO_THICKEN_SIDE_ONE,NULL);
        if(!error)error=model_element(tx.root,PRO_E_THICKNESS,2,2,NULL);
        if(!error)error=model_element(tx.root,PRO_E_SRF_OFFS_METHOD,1,PRO_OFFS_METH_NORMTOSURF,NULL);
    }else if(!error){
        error=fd_ref_resolve(model,L"PLANE:PLATE_PLANE",PRO_SURFACE,&reference);
        if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_PATCH_QUILT,&reference);
        if(!error)error=model_element(tx.root,PRO_E_PATCH_TYPE,1,PRO_FEAT_CUT,NULL);
        if(!error)error=model_element(tx.root,PRO_E_PATCH_MATERIAL_SIDE,1,PRO_SOLIDIFY_SIDE_ONE,NULL);
    }
    if(!error)error=model_element(tx.root,PRO_E_BODY,0,0,&body);
    if(!error)error=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL);
    if(!error)error=sd_existing_body(&tx,body);
    if(!error)error=fd_create(&tx,0);
    if(!error)error=part_regenerate(model);
    fd_ref_free(&reference);
    fd_cleanup(&tx,error);
    return error;
}

static ProError quilt_solid_source(ProMdl model,const wchar_t *name,int variant,FILE *log)
{
    const wchar_t *plate[]={
        L"DATUM_PLANE %ls PLATE_PLANE PLANE:FRONT 8",
        L"SKETCH_CREATE %ls PLATE_SK PLANE:PLATE_PLANE PLANE:RIGHT RECT 0 0 20 10",
        L"SURFACE_FILL %ls PATCH PLATE_SK"
    };
    const wchar_t *closed[]={
        L"SKETCH_CREATE %ls WALL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",
        L"SURFACE_EXTRUDE %ls SIDES WALL_SK 10 POS",
        L"SURFACE_FILL %ls CAP_TOP WALL_SK",
        L"SURFACE_OFFSET %ls CAP_BOTTOM QUILT:CAP_TOP -10",
        L"SURFACE_MERGE %ls CLOSED QUILT:SIDES QUILT:CAP_TOP QUILT:CAP_BOTTOM",
        L"SURFACE_SOLIDIFY %ls QUILT_SOLID QUILT:SIDES"
    };
    if(variant==3)return quilt_execute_steps(model,name,closed,_countof(closed),log);
    ProError error=quilt_execute_steps(model,name,plate,variant==5?1:_countof(plate),log);
    if(variant>=4)return error?error:quilt_cut_source(model,variant);
    const wchar_t *directions[]={L"POS",L"NEG",L"SYM"};
    wchar_t command[512];
    swprintf_s(command,_countof(command),L"CREO_OP SURFACE_THICKEN %ls QUILT_SOLID QUILT:PATCH 2 %ls",name,directions[variant]);
    return error?error:expansion_build(model,command,log);
}

static ProError quilt_solid_edit(ProMdl model,int variant,double initial_volume,FILE *log)
{
    ProFeature feature;
    QuiltDimension driver={.value=variant==5?8:variant==3?20:2};
    ProMassProperty before={0},after={0},restored={0};
    const double modified=variant==5?9:variant==3?24:3;
    const double delta=variant==5?4800:variant==3?400:200;
    ProError error=ux_named_feature(model,variant==5?L"PLATE_PLANE":variant==3?L"WALL_SK":L"QUILT_SOLID",&feature);
    if(!error)error=ProFeatureDimensionVisit(&feature,quilt_dimension,NULL,&driver);
    if(!error&&driver.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(model,&before);
    if(!error&&fabs(before.volume-initial_volume)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&driver.item,modified);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&after);
    if(!error&&fabs((variant==5?fabs(after.volume-before.volume):
        (variant==4?before.volume-after.volume:after.volume-before.volume))-delta)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&driver.item,driver.value);
    if(!error)error=part_regenerate(model);
    if(!error)error=qa_geometry_mass(model,&restored);
    if(!error&&fabs(restored.volume-initial_volume)>0.01)error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"QUILT SOLID EDIT VARIANT=%d ERROR=%d DRIVERS=%d VOLUME=%.6f/%.6f/%.6f\n",
        variant,error,driver.count,before.volume,after.volume,restored.volume);
    return error;
}

static ProError quilt_solid_trial(int variant,FILE *log)
{
    ProMdl source=NULL,rebuilt=NULL;
    ProMdlName name;
    const char *stage="prepare quilt material fixture";
    Fixture stock;
    ProError error=variant>=4?fixture(&source,name,&stock):model_create_empty(1,name,&stage);
    if(!error&&!source)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    if(!error)error=quilt_solid_source(source,name,variant,log);
    ProMassProperty mass={0};
    if(!error)error=qa_geometry_mass(source,&mass);
    if(!error&&variant==4&&fabs(mass.volume-95600)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error&&variant==5&&fabs(mass.volume-38400)>0.01&&fabs(mass.volume-57600)>0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProMdlSave(source);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"QUILT SOLID REBUILD VARIANT=%d ERROR=%d\n%ls\n",variant,error,report);
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&rebuilt);
    ProFeature feature;
    ProElement tree=NULL;
    int form,side;
    const wchar_t *operation=NULL;
    if(!error)error=ux_named_feature(rebuilt,L"QUILT_SOLID",&feature);
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error)error=replay_tree_integer(tree,PRO_E_FEATURE_FORM,&form);
    if(!error&&(form!=PRO_USE_SURFS||!replay_quilt_solid_definition(tree,variant>=4?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION,&operation)))error=PRO_TK_INCOMPLETE;
    if(!error&&variant<3){
        const int expected[]={PRO_THICKEN_SIDE_ONE,PRO_THICKEN_SIDE_TWO,PRO_THICKEN_BOTH_SIDES};
        error=replay_tree_integer(tree,PRO_E_STD_USEQLT_SIDE,&side);
        if(!error&&side!=expected[variant])error=PRO_TK_INCOMPLETE;
    }
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    if(!error)error=quilt_solid_edit(rebuilt,variant,mass.volume,log);
    return error;
}

static int quilt_solid_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-quilt-solid.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    ProError results[6];
    for(int i=0;i<_countof(results);i++)results[i]=quilt_solid_trial(i,log);
    fwprintf(log,L"POS=%d NEG=%d SYM=%d SOLID=%d THICKEN_CUT=%d SOLIDIFY_CUT=%d PASS=%d\n",
        results[0],results[1],results[2],results[3],results[4],results[5],
        !results[0]&&!results[1]&&!results[2]&&!results[3]&&!results[4]&&!results[5]);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLReplayQuiltSolid",quilt_solid_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayQuiltSolid\x0060;");
}
