static ProError sf_extend(FdTxn *tx,const PartCommand *cmd){
    FdRef quilt={0},edge={0};
    ProElement collection=NULL;
    ProEdge geometry=NULL,next1=NULL,next2=NULL;
    ProSurface face1=NULL,face2=NULL;
    ProQuilt owner=NULL;
    ProGeomitem owner_item;
    ProError e;
    if((e=sf_ref(tx->model,cmd->arg[3],&quilt))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_EDGE,&edge)))goto done;
    if((e=ProGeomitemToEdge(&edge.item,&geometry))||
       (e=ProEdgeNeighborsGet(geometry,&next1,&next2,&face1,&face2)))goto done;
    if((face1&&face2)||(!face1&&!face2)){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=ProSurfaceQuiltGet((ProSolid)tx->model,face1?face1:face2,&owner))||
       (e=ProQuiltToGeomitem((ProSolid)tx->model,owner,&owner_item)))goto done;
    if(owner_item.id!=quilt.item.id){e=PRO_TK_BAD_INPUTS;goto done;}
    if(
       (e=model_element(tx->root,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collection))||
       (e=sd_curve_collection(collection,&edge)))goto done;
    tx->parents[tx->parent_count++]=edge.owner_feature;
    if((e=model_element(tx->root,PRO_E_SURF_EXTEND_TYPE,1,2,NULL))||
       (e=model_element(tx->root,PRO_E_SURF_EXTEND_DIST_STATE,1,1,NULL))||
       (e=model_element(tx->root,PRO_E_SURF_EXTEND_WAY_OF_MEASURING,1,1,NULL))||
       (e=model_element(tx->root,PRO_E_SURF_EXTEND_FLIP,1,1,NULL)))goto done;
    e=model_element(tx->root,PRO_E_SURF_EXTEND_SINGLE_DIST_VALUE,2,as_arg(cmd,5)*tx->scale,NULL);
done:
    fd_ref_free(&quilt);fd_ref_free(&edge);
    return e;
}
