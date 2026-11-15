#include <ProSplitbody.h>

static int replay_split_definition(ProElement tree)
{
    int method,operation,side,extend;
    ProElement branch,element;
    return !replay_tree_integer(tree,PRO_E_SPLIT_BY_OPTION,&method)&&method==PRO_SPLIT_BY_SPLITTING_OBJ&&
        !replay_tree_integer(tree,PRO_E_SPLIT_BODY_TYPE,&operation)&&operation==PRO_SPLIT_BODY&&
        !mc_tree_element(tree,(ProElemId[]){PRO_E_SPLIT_BODY},1,&branch)&&
        !mc_tree_element(branch,(ProElemId[]){PRO_E_SPLIT_TARGET_BODY},1,&element)&&
        !mc_tree_element(branch,(ProElemId[]){PRO_E_SPLIT_BODY_REF},1,&element)&&
        !replay_tree_integer(branch,PRO_E_SPLIT_BODY_SLICE_OPT,&side)&&
        (side==PRO_E_SPLIT_BODY_SLICE_FIRST_OPT||side==PRO_E_SPLIT_BODY_SLICE_SECOND_OPT)&&
        !replay_tree_integer(branch,PRO_E_SPLIT_TOOL_EXTEND_OPT,&extend)&&
        (extend==PRO_E_SPLIT_TOOL_EXTEND_OPT_NO||extend==PRO_E_SPLIT_TOOL_EXTEND_OPT_YES);
}

static ProError replay_split(ImitationReplay *replay,ProElement original,const wchar_t *name,ProFeature *created)
{
    FdTxn tx={0};
    FdRef target={0};
    ProElement input=NULL,branch=NULL,old_plane=NULL;
    ProReference source_ref=NULL,plane_ref=NULL;
    ProGeomitem plane;
    ProBoolean datum=PRO_B_FALSE;
    ProSurface surface;
    int before=0,after=0,side,extend;
    ProError error=sd_body_count(replay->target,&before);
    if(!error&&(before!=1||!replay_split_definition(original)))error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(original,(ProElemId[]){PRO_E_SPLIT_BODY},1,&input);
    if(!error)error=replay_tree_integer(input,PRO_E_SPLIT_BODY_SLICE_OPT,&side);
    if(!error)error=replay_tree_integer(input,PRO_E_SPLIT_TOOL_EXTEND_OPT,&extend);
    if(!error)error=replay_body_by_origin(replay,input,PRO_E_SPLIT_TARGET_BODY,&target);
    if(!error)error=mc_tree_element(input,(ProElemId[]){PRO_E_SPLIT_BODY_REF},1,&old_plane);
    if(!error)error=ProElementReferenceGet(old_plane,NULL,&source_ref);
    if(!error)error=replay_map_reference(replay,source_ref,&plane_ref);
    ProSelection selection=NULL;
    if(!error)error=ProReferenceToSelection(plane_ref,&selection);
    if(!error)error=ProSelectionModelitemGet(selection,&plane);
    if(!error)error=ProGeomitemToSurface(&plane,&surface);
    if(!error)error=ProSurfaceIsDatumPlane((ProSolid)replay->target,surface,&datum);
    if(!error&&!datum)error=PRO_TK_UNSUPPORTED;
    if(!error)error=sh_begin(&tx,replay->target,name,PRO_FEAT_SPLIT_BODY);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BY_OPTION,1,PRO_SPLIT_BY_SPLITTING_OBJ,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BODY_TYPE,1,PRO_SPLIT_BODY,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SPLIT_BODY,0,0,&branch);
    if(!error)error=fd_ref_element(&tx,branch,PRO_E_SPLIT_TARGET_BODY,&target);
    if(!error)error=fd_ref_element(&tx,branch,PRO_E_SPLIT_BODY_REF,&(FdRef){.selection=selection});
    if(!error)error=model_element(branch,PRO_E_SPLIT_BODY_SLICE_OPT,1,side,NULL);
    if(!error)error=model_element(branch,PRO_E_SPLIT_TOOL_EXTEND_OPT,1,extend,NULL);
    if(!error)error=fd_create(&tx,0);
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=sd_body_count(replay->target,&after);
    if(!error&&after!=before+1)error=PRO_TK_INCOMPLETE;
    if(!error)*created=tx.feature;
    if(selection)ProSelectionFree(&selection);
    if(plane_ref)ProReferenceFree(plane_ref);
    if(source_ref)ProReferenceFree(source_ref);
    fd_ref_free(&target);
    fd_cleanup(&tx,PRO_TK_NO_ERROR);
    return error;
}
