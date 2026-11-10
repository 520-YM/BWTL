static ProError mc_hole_place(FdTxn *tx,const PartCommand *cmd,McHoleGeometry *geometry,int *face_id){
    FdRef face={0},r1={0},r2={0};ProElement placement=NULL,body=NULL;
    ProVector p[3],n[3];double x,y,dot;ProError e;ProFeattype ft;int i,j;
#define MH(call) do{e=(call);if(e)goto done;}while(0)
    tx->stage="resolve planar hole placement and orthogonal offsets";
    MH(fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&face));
    MH(fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&r1));
    MH(fd_ref_resolve(tx->model,cmd->arg[6],PRO_SURFACE,&r2));
    if(face.item.id==r1.item.id||face.item.id==r2.item.id||r1.item.id==r2.item.id){e=PRO_TK_BAD_INPUTS;goto done;}
    {ProFeature f;MH(ProGeomitemFeatureGet(&face.item,&f));MH(ProFeatureTypeGet(&f,&ft));if(ft==PRO_FEAT_DATUM){e=PRO_TK_BAD_INPUTS;goto done;}}
    MH(fd_plane_data(&face.item,p[0],n[0]));MH(fd_plane_data(&r1.item,p[1],n[1]));MH(fd_plane_data(&r2.item,p[2],n[2]));
    for(i=0;i<3;i++)for(j=i+1;j<3;j++){dot=n[i][0]*n[j][0]+n[i][1]*n[j][1]+n[i][2]*n[j][2];if(fabs(dot)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}}
    part_number(cmd->arg[5],&x);part_number(cmd->arg[7],&y);
    x*=tx->scale;y*=tx->scale;
    if(face_id)*face_id=face.item.id;
    for(i=0;i<3;i++){
        geometry->center[i]=x*n[1][i]+y*n[2][i];geometry->normal[i]=n[0][i];
        for(j=0;j<3;j++)geometry->center[i]+=(p[j][0]*n[j][0]+p[j][1]*n[j][1]+p[j][2]*n[j][2])*n[j][i];
    }
    MH(model_element(tx->root,PRO_E_HLE_PLACEMENT,0,0,&placement));
    MH(fd_ref_element(tx,placement,PRO_E_HLE_PRIM_REF,&face));
    MH(model_element(placement,PRO_E_HLE_PL_TYPE,1,PRO_HLE_PL_TYPE_LIN,NULL));
    MH(fd_ref_element(tx,placement,PRO_E_HLE_DIM_REF1,&r1));
    MH(model_element(placement,PRO_E_HLE_PLC_ALIGN_OPT1,1,x==0?PRO_HLE_PLC_ALIGN:PRO_HLE_PLC_NOT_ALIGN,NULL));
    MH(model_element(placement,PRO_E_HLE_DIM_DIST1,2,x,NULL));
    MH(fd_ref_element(tx,placement,PRO_E_HLE_DIM_REF2,&r2));
    MH(model_element(placement,PRO_E_HLE_PLC_ALIGN_OPT2,1,y==0?PRO_HLE_PLC_ALIGN:PRO_HLE_PLC_NOT_ALIGN,NULL));
    MH(model_element(placement,PRO_E_HLE_DIM_DIST2,2,y,NULL));
    MH(model_element(tx->root,PRO_E_BODY,0,0,&body));MH(model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_ALL,NULL));
done:fd_ref_free(&face);fd_ref_free(&r1);fd_ref_free(&r2);return e;
#undef MH
}

static ProError mc_hole(FdTxn *tx,const PartCommand *cmd,McHoleGeometry *geometry){
    ProElement com=NULL,depth=NULL,to=NULL,from=NULL;
    double diameter,drill_depth;
    int blind=!wcscmp(cmd->arg[9],L"BLIND");
    ProError e;
    part_number(cmd->arg[8],&diameter);part_number(cmd->arg[10],&drill_depth);
    diameter*=tx->scale;drill_depth*=tx->scale;
    geometry->radius=diameter/2;
    tx->stage="build independent native straight hole";
    if((e=model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_HLE_TYPE_STRAIGHT,NULL))||
       (e=model_element(tx->root,PRO_E_HLE_COM,0,0,&com))||
       (e=model_element(com,PRO_E_HLE_TYPE_NEW,1,PRO_HLE_NEW_TYPE_STRAIGHT,NULL))||
       (e=model_element(com,PRO_E_HLE_MAKE_LIGHTWT,1,PRO_HLE_REGULAR,NULL))||
       (e=model_element(com,PRO_E_HLE_TOP_CLEARANCE,1,PRO_HOLE_GEN_CLRNCE,NULL))||
       (e=model_element(com,PRO_E_HLE_ADD_PARAMETERS,1,PRO_HOLE_NO_PARAMETERS_FLAG,NULL))||
       (e=model_element(com,PRO_E_HLE_ADD_NOTE,1,PRO_HOLE_NO_NOTE_FLAG,NULL))||
       (e=model_element(com,PRO_E_DIAMETER,2,diameter,NULL))||
       (e=model_element(com,PRO_E_HOLE_STD_DEPTH,0,0,&depth))||
       (e=model_element(depth,PRO_E_HOLE_DEPTH_TO,0,0,&to))||
       (e=model_element(to,PRO_E_HOLE_DEPTH_TO_TYPE,1,blind?PRO_HLE_STRGHT_BLIND_DEPTH:PRO_HLE_STRGHT_THRU_ALL_DEPTH,NULL))||
       (blind&&(e=model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,drill_depth,NULL)))||
       (e=model_element(depth,PRO_E_HOLE_DEPTH_FROM,0,0,&from))||
       (e=model_element(from,PRO_E_HOLE_DEPTH_FROM_TYPE,1,PRO_HLE_STRGHT_NONE_DEPTH,NULL))||
       (e=mc_hole_place(tx,cmd,geometry,NULL)))return e;
    return fd_create(tx,0);
}
