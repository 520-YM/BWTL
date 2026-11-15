#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError stepped_hole_draw(ProSection profile)
{
    Pro2dLinedef axis={0};
    axis.type=PRO_2D_CENTER_LINE;
    axis.end1[1]=-18;
    axis.end2[1]=1;
    int id;
    ProBoolean intent=PRO_B_FALSE;
    ProError error=ProSectionIntentManagerModeGet(profile,&intent);
    if(!error&&intent)error=ProSectionIntentManagerModeSet(profile,PRO_B_FALSE);
    if(!error)error=model_section_origin(profile);
    trace("step profile mode",error);
    if(!error)error=ProSectionEntityAdd(profile,(Pro2dEntdef *)&axis,&id);
    trace("step profile axis",error);
    const AsPoint shape[]={{0,0},{8,0},{8,-4},{5,-4},{5,-9},{3,-9},{3,-16},{0,-16},{0,0}};
    for(int i=0;!error&&i+1<_countof(shape);i++){
        error=as_add_line(profile,shape[i],shape[i+1],&id);
        if(!error&&i==5){
            int dimension;
            ProSectionPointType point=PRO_ENT_WHOLE;
            error=fd_dimension_create(profile,PRO_TK_DIM_LINE,1,&id,&point,7,1,&dimension);
            trace("step profile dimension",error);
        }
    }
    if(!error){
        ProWSecerror diagnostics=NULL;
        error=ProSecerrorAlloc(&diagnostics);
        if(!error)error=ProSectionEpsilonSet(profile,1e-5);
        if(!error)error=ProSectionAutodim(profile,&diagnostics);
        trace("step profile autodim",error);
        if(!error)error=ProSectionRegenerate(profile,&diagnostics);
        if(diagnostics)ProSecerrorFree(&diagnostics);
    }
    return error;
}

static ProError stepped_hole_fixture(ProMdl source,const ProMdlName name,const Fixture *faces)
{
    FdTxn tx={0};
    ProError error=sh_begin(&tx,source,L"STEPPED_HOLE",PRO_FEAT_HOLE);
    ProElement hole=NULL,section=NULL;
    ProSection profile=NULL;
    int attached=0;
    if(!error)error=model_element(tx.root,PRO_E_FEATURE_FORM,1,PRO_HLE_TYPE_SKETCHED,NULL);
    if(!error)error=model_element(tx.root,PRO_E_HLE_COM,0,0,&hole);
    if(!error)error=model_element(hole,PRO_E_HLE_TYPE_NEW,1,PRO_HLE_NEW_TYPE_SKETCH,NULL);
    if(!error)error=model_element(hole,PRO_E_HLE_MAKE_LIGHTWT,1,PRO_HLE_REGULAR,NULL);
    if(!error)error=model_element(hole,PRO_E_HLE_CRDIR_FLIP,1,PRO_HLE_CR_IN_SIDE_ONE,NULL);
    if(!error)error=model_element(hole,PRO_E_HLE_TOP_CLEARANCE,1,PRO_HOLE_GEN_CLRNCE,NULL);
    wchar_t protocol[512];
    PartCommand command={0};
    swprintf_s(protocol,_countof(protocol),L"CREO_OP HOLE %ls STEPPED_HOLE SURFACE:%d SURFACE:%d 30 SURFACE:%d 25 6 THROUGH 0",
        name,faces->top,faces->x,faces->y);
    if(!error&&part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
    McHoleGeometry geometry={0};
    if(!error)error=mc_hole_place(&tx,&command,&geometry,NULL);
    if(!error)error=ProSection2DAlloc(&profile);
    if(!error)error=stepped_hole_draw(profile);
    trace("stepped hole sketch",error);
    if(!error)error=replay_hole_profile_attach(tx.root,profile,&section);
    if(!error)attached=1;
    if(!error)error=fd_create(&tx,0);
    trace("stepped hole feature",error);
    for(int i=0;tx.errors.error_list&&i<tx.errors.error_number;i++){
        trace("stepped hole element",tx.errors.error_list[i].err_item_id);
        trace("stepped hole element error",tx.errors.error_list[i].error);
    }
    if(!error)error=part_regenerate(source);
    if(attached&&ProElementSpecialvalueSet(section,NULL))profile=NULL;
    fd_cleanup(&tx,error);
    if(profile)ProSectionFree(&profile);
    return error;
}

static ProError stepped_hole_trial(FILE *log)
{
    ProMdl source=NULL;
    ProMdlName name;
    Fixture faces;
    ProError error=fixture(&source,name,&faces);
    if(!error)error=stepped_hole_fixture(source,name,&faces);
    fwprintf(log,L"STEPPED HOLE SOURCE=%d\n",error);
    ProPath output=L"";
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&!wcsstr(report,L"草绘台阶孔"))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"STEPPED HOLE PREVIEW=%d\n%ls\n",error,report);
    report[0]=0;
    out.truncated=0;
    if(!error)error=imitation_run_mode(source,output,0,1,&out);
    fwprintf(log,L"STEPPED HOLE REPLAY=%d\n%ls\n",error,report);
    ProMdl result=NULL;
    if(!error)error=ProMdlFiletypeLoad(output,PRO_MDLFILE_PART,PRO_B_FALSE,&result);
    ProFeature feature;
    if(!error)error=ux_named_feature(result,L"STEPPED_HOLE",&feature);
    ProElement tree=NULL;
    if(!error)error=ProFeatureElemtreeExtract(&feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error&&!replay_is_hole_profile(tree))error=PRO_TK_INCOMPLETE;
    if(tree)ProFeatureElemtreeFree(&feature,tree);
    ProSolidBody body;
    if(!error)error=qa_single_body(result,&body);
    ImitationDiameterRead first={1,16,10,0,0},last={1,6,0,0,0};
    if(!error)error=ProSolidBodySurfaceVisit(&body,imitation_cylinder_diameter,&first);
    if(!error)error=ProSolidBodySurfaceVisit(&body,imitation_cylinder_diameter,&last);
    if(!error&&(!first.old_count||!first.new_count||!last.old_count))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"STEPPED HOLE THREE DIAMETERS=%d\n",error);
    ReplayDimensionMatch edit={0};
    edit.type=PRODIMTYPE_LINEAR;
    edit.value=7;
    ProMassProperty before,after,restored;
    if(!error)error=ProFeatureDimensionVisit(&feature,replay_match_dimension,NULL,&edit);
    if(!error&&edit.count!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=qa_geometry_mass(result,&before);
    if(!error)error=ProDimensionValueSet(&edit.target,7.5);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&after);
    if(!error&&fabs(after.volume-before.volume)<0.01)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProDimensionValueSet(&edit.target,7);
    if(!error)error=part_regenerate(result);
    if(!error)error=qa_geometry_mass(result,&restored);
    if(!error&&!part_close(restored.volume,before.volume))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"STEPPED HOLE EDIT AND RESTORE=%d\n",error);
    return error;
}

static int stepped_hole_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-stepped-hole.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=stepped_hole_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplaySteppedHole",stepped_hole_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplaySteppedHole\x0060;");
}
