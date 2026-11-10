static ProError rf_draft(FdTxn *tx,const PartCommand *cmd){
    FdRef face={0},hinge={0},direction={0};
    ProElement surfaces=NULL,dir=NULL,side=NULL;
    ProCollection collection=NULL;
    ProVector point,normal,hinge_normal,dir_normal;
    double angle,dot=0,hinge_dot=0;
    int i,positive=rf_side(cmd->arg[7])&&!wcscmp(cmd->arg[7],L"POS");
    ProError e;
#define DRAFT_STEP(call) do{e=(call);trace(#call,e);if(e)goto done;}while(0)
    tx->stage="resolve real draft face, hinge and direction";
    DRAFT_STEP(fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&face));
    DRAFT_STEP(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&hinge));
    DRAFT_STEP(fd_ref_resolve(tx->model,cmd->arg[5],PRO_SURFACE,&direction));
    e=ProSolidSurfaceVisit((ProSolid)tx->model,mc_find_shell_face,NULL,&face.item.id);
    if(e!=PRO_TK_E_FOUND){if(!e)e=PRO_TK_BAD_CONTEXT;goto done;}
    DRAFT_STEP(fd_plane_data(&face.item,point,normal));
    DRAFT_STEP(fd_plane_data(&hinge.item,point,hinge_normal));
    DRAFT_STEP(fd_plane_data(&direction.item,point,dir_normal));
    for(i=0;i<3;i++){dot+=normal[i]*dir_normal[i];hinge_dot+=hinge_normal[i]*dir_normal[i];}
    if(fabs(dot)>1e-6||fabs(fabs(hinge_dot)-1)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}
    part_number(cmd->arg[6],&angle);
    tx->stage="build constant native draft";
    DRAFT_STEP(model_element(tx->root,PRO_E_DRAFT_SPLIT,1,PRO_DRAFT_UI_SPLIT_NONE,NULL));
    DRAFT_STEP(model_element(tx->root,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&surfaces));
    DRAFT_STEP(ProSrfcollectionAlloc(&collection));
    DRAFT_STEP(rf_surface_add(tx,collection,&face));
    DRAFT_STEP(ProElementCollectionSet(surfaces,collection));
    DRAFT_STEP(model_element(tx->root,PRO_E_DIRECTION_COMPOUND,0,0,&dir));
    DRAFT_STEP(fd_ref_element(tx,dir,PRO_E_DIRECTION_REFERENCE,&direction));
    DRAFT_STEP(model_element(dir,PRO_E_DIRECTION_FLIP,1,positive?PRO_DIRECTION_FLIP_ALONG:PRO_DIRECTION_FLIP_AGAINST,NULL));
    DRAFT_STEP(model_element(tx->root,PRO_E_DRAFT_CONSTANT_OR_VARIABLE,1,PRO_DRAFT_UI_CONSTANT,NULL));
    DRAFT_STEP(model_element(tx->root,PRO_E_DRAFT_INCLUDE_TANGENT,1,PRO_DRAFT_UI_NOT_INC_TANG,NULL));
    DRAFT_STEP(model_element(tx->root,PRO_E_DRAFT_SIDE_1,0,0,&side));
    DRAFT_STEP(model_element(side,PRO_E_DRAFT_NEUTRAL_OBJECT_TYPE_1,1,PRO_DRAFT_UI_PLANE,NULL));
    DRAFT_STEP(fd_ref_element(tx,side,PRO_E_DRAFT_NEUTRAL_PLANE_1,&hinge));
    DRAFT_STEP(model_element(side,PRO_E_DRAFT_DEPENDENT_1,1,PRO_DRAFT_UI_INDEPENDENT,NULL));
    DRAFT_STEP(model_element(side,PRO_E_DRAFT_ANGLE_1,2,angle,NULL));
    DRAFT_STEP(fd_create(tx,0));
    tx->stage="verify native draft angle and direction";
    {
        const ProElemId angle_path[]={PRO_E_DRAFT_SIDE_1,PRO_E_DRAFT_ANGLE_1};
        const ProElemId flip[]={PRO_E_DIRECTION_COMPOUND,PRO_E_DIRECTION_FLIP};
        DRAFT_STEP(mc_tree_value(&tx->feature,angle_path,2,angle,0));
        DRAFT_STEP(mc_tree_value(&tx->feature,flip,2,positive?PRO_DIRECTION_FLIP_ALONG:PRO_DIRECTION_FLIP_AGAINST,1));
    }
done:
    if(collection){trace("release draft collection",0);ProCollectionFree(&collection);}
    fd_ref_free(&face);fd_ref_free(&hinge);fd_ref_free(&direction);
    return e;
#undef DRAFT_STEP
}
