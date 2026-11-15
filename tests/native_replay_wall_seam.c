#define BWTL_REUSE_BATCH_SHAPES
#include "native_replay_batch_shapes.c"
#undef BWTL_REUSE_BATCH_SHAPES
#include <ProSmtSplitArea.h>

static ProError wire_seam_surface(ProSurface surface,ProError status,ProAppData data)
{
    BatchFace *face=data;
    if(status)return status;
    ProGeomitem item;
    ProSmtSurfType type;
    ProVector position,normal;
    if(!ProSurfaceToGeomitem((ProSolid)face->model,surface,&item)&&
       !ProSmtSurfaceTypeGet((ProPart)face->model,surface,&type)&&type==PRO_SMT_SURF_FACE&&
       !fd_plane_data(&item,position,normal)&&fabs(normal[2])>.999)face->face=item.id;
    return PRO_TK_NO_ERROR;
}

static ProError wall_split_trial(ProMdl model,const wchar_t *name,FILE *log)
{
    BatchFace face={model,0};
    ProError error=ProSolidSurfaceVisit((ProSolid)model,wire_seam_surface,NULL,&face);
    if(!error&&!face.face)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=batch_step(model,log,L"CREO_OP SMT_SKETCH %ls SEAM_SK SURFACE:%d PLANE:RIGHT RECT 12 12 20 12",name,face.face);
    if(error)return error;
    FdTxn tx={0};
    FdRef sketch={0};
    ProSectionShape shape;
    ProElement section;
    error=sh_begin(&tx,model,L"WALL_SPLIT",PRO_FEAT_DEFORM_AREA);
    if(!error)error=sd_sketch_ref(model,L"SEAM_SK",&sketch,&shape);
    if(!error)error=model_element(tx.root,PRO_E_STD_SECTION,0,0,&section);
    if(!error)error=fd_ref_element(&tx,section,PRO_E_SEC_USE_SKETCH,&sketch);
    if(!error)error=model_element(tx.root,PRO_E_SMT_CUT_NORMAL_DIR,1,PRO_SMT_CUT_DRVSIDE_GREEN,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_PROJ_DIR,1,PRO_SMT_PROJ_DIR_BOTH,NULL);
    if(!error)error=model_element(tx.root,PRO_E_STD_MATRLSIDE,1,PRO_SPLITAREA_MATSIDE_ONE,NULL);
    if(!error)error=fd_create(&tx,0);
    fwprintf(log,L"WALL SPLIT CREATE=%d\n",error);
    if(!error)error=part_regenerate(model);
    fd_ref_free(&sketch);
    fd_cleanup(&tx,error);
    return error;
}

static int wall_seam_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-wall-seam.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    ProMdl source=NULL,result=NULL;
    ProMdlName name;
    model_background_mode=1;
    ProError error=batch_sheet(&source,name,log);
    if(!error)error=wall_split_trial(source,name,log);
    if(!error)error=batch_reopen(source,&result,log);
    if(!error){
        ProFeature split,geometry;
        ReplayDimensionMatch width={.type=PRODIMTYPE_LINEAR,.value=20};
        ProMassProperty before,edited,restored;
        error=ux_named_feature(result,L"WALL_SPLIT",&split);
        if(!error)error=ux_named_feature(result,L"SEAM_SK",&geometry);
        if(!error)error=ProFeatureDimensionVisit(&geometry,replay_match_dimension,NULL,&width);
        if(!error&&width.count!=1)error=PRO_TK_INCOMPLETE;
        if(!error)error=qa_geometry_mass(result,&before);
        if(!error)error=ProDimensionValueSet(&width.target,22);
        if(!error)error=part_regenerate(result);
        if(!error)error=part_health(result);
        if(!error)error=qa_geometry_mass(result,&edited);
        if(!error&&!part_close(before.volume,edited.volume))error=PRO_TK_INCOMPLETE;
        if(!error)error=ProDimensionValueSet(&width.target,20);
        if(!error)error=part_regenerate(result);
        if(!error)error=qa_geometry_mass(result,&restored);
        if(!error&&!part_close(before.volume,restored.volume))error=PRO_TK_INCOMPLETE;
        fwprintf(log,L"EDIT SPLIT 20 -> 22 -> 20 ERROR=%d\n",error);
    }
    fwprintf(log,L"WALL SPLIT RESULT=%d PASS=%d\n",error,!error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayWallSeam",wall_seam_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayWallSeam\x0060;");
}

