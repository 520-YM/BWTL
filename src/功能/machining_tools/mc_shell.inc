static ProError mc_find_shell_face(ProSurface surface,ProError status,ProAppData data){
    int id;
    ProError e;
    if(status)return status;
    if((e=ProSurfaceIdGet(surface,&id)))return e;
    return id==*(int *)data?PRO_TK_E_FOUND:PRO_TK_NO_ERROR;
}

static ProError mc_shell(FdTxn *tx,const PartCommand *cmd,int *opening){
    FdRef face={0};
    ProElement body=NULL;
    ProVector point,normal;
    double thickness;
    ProError e;
    part_number(cmd->arg[4],&thickness);
    tx->stage="resolve actual planar shell opening";
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&face))||
       (e=fd_plane_data(&face.item,point,normal)))goto done;
    *opening=face.item.id;
    e=ProSolidSurfaceVisit((ProSolid)tx->model,mc_find_shell_face,NULL,opening);
    if(e!=PRO_TK_E_FOUND){if(!e)e=PRO_TK_BAD_CONTEXT;goto done;}
    tx->stage="build uniform inward native shell";
    if((e=model_element(tx->root,PRO_E_BODY,0,0,&body))||
       (e=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL))||
       (e=sd_existing_body(tx,body))||
       (e=fd_ref_element(tx,tx->root,PRO_E_SHELL_SRF,&face))||
       (e=model_element(tx->root,PRO_E_SHELL_THICK,2,thickness*tx->scale,NULL))||
       (e=model_element(tx->root,PRO_E_SHELL_FLIP,1,PRO_SHELL_INSIDE,NULL)))goto done;
    e=fd_create(tx,0);
done:
    fd_ref_free(&face);
    return e;
}

static ProError mc_verify_shell(FdTxn *tx,double thickness,int opening){
    const ProElemId thick[]={PRO_E_SHELL_THICK},side[]={PRO_E_SHELL_FLIP},face[]={PRO_E_SHELL_SRF};
    ProElement tree=NULL,element=NULL;
    ProReference *refs=NULL;
    int count,id;
    ProError e;
    tx->stage="verify shell thickness, direction and opening reference";
    if((e=mc_tree_value(&tx->feature,thick,1,thickness,0))||
       (e=mc_tree_value(&tx->feature,side,1,PRO_SHELL_INSIDE,1))||
       (e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||
       (e=mc_tree_element(tree,face,1,&element))||
       (e=ProElementReferencesGet(element,NULL,&refs))||
       (e=ProArraySizeGet((ProArray)refs,&count)))goto done;
    if(count!=1){e=PRO_TK_GENERAL_ERROR;goto done;}
    if(!(e=ProReferenceIdGet(refs[0],&id))&&id!=opening)e=PRO_TK_GENERAL_ERROR;
done:
    if(refs)ProReferencearrayFree(refs);
    if(tree)ProFeatureElemtreeFree(&tx->feature,tree);
    return e;
}
