typedef struct {ProVector center,axis;double radius;int known,walls;} AmHole;
static ProError am_flat_hole(FdTxn *tx,double diameter,double depth){
    ProElement com=NULL,limits=NULL,to=NULL,from=NULL,body=NULL;ProError e;
    Pro3dPnt bounds[2];double diagonal=0;
    if((e=ProSolidOutlineGet((ProSolid)tx->model,bounds)))return e;
    for(int i=0;i<3;i++)diagonal+=pow(bounds[1][i]-bounds[0][i],2);
    if(diameter>=sqrt(diagonal)/2)return PRO_TK_BAD_INPUTS;
    if((e=model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_HLE_TYPE_STRAIGHT,NULL))||
       (e=model_element(tx->root,PRO_E_HLE_COM,0,0,&com))||
       (e=model_element(com,PRO_E_HLE_TYPE_NEW,1,PRO_HLE_NEW_TYPE_STRAIGHT,NULL))||
       (e=model_element(com,PRO_E_HLE_MAKE_LIGHTWT,1,PRO_HLE_REGULAR,NULL))||
       (e=model_element(com,PRO_E_HLE_TOP_CLEARANCE,1,PRO_HOLE_GEN_CLRNCE,NULL))||
       (e=model_element(com,PRO_E_DIAMETER,2,diameter,NULL))||
       (e=model_element(com,PRO_E_HOLE_STD_DEPTH,0,0,&limits))||
       (e=model_element(limits,PRO_E_HOLE_DEPTH_TO,0,0,&to))||
       (e=model_element(to,PRO_E_HOLE_DEPTH_TO_TYPE,1,depth>0?PRO_HLE_STRGHT_BLIND_DEPTH:PRO_HLE_STRGHT_THRU_ALL_DEPTH,NULL))||
       (depth>0&&(e=model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,depth,NULL)))||
       (e=model_element(limits,PRO_E_HOLE_DEPTH_FROM,0,0,&from))||
       (e=model_element(from,PRO_E_HOLE_DEPTH_FROM_TYPE,1,PRO_HLE_STRGHT_NONE_DEPTH,NULL))||
       (e=model_element(tx->root,PRO_E_BODY,0,0,&body)))return e;
    return model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_ALL,NULL);
}
static ProError am_hole_wall(ProGeomitem *item,ProError status,ProAppData data){
    AmHole *hole=data;ProSurface face;ProSrftype type;ProGeomitemdata *raw=NULL;ProError e;
    ProVector delta,cross;ProCylinderdata *c;
    if(status)return status;
    if((e=ProGeomitemToSurface(item,&face))||(e=ProSurfaceTypeGet(face,&type)))return e;
    if(type!=PRO_SRF_CYL)return 0;
    if((e=ProSurfaceDataGet(face,&raw)))return e;
    c=&raw->data.p_surface_data->srf_shape.cylinder;
    if(!part_close(c->radius,hole->radius))goto done;
    if(hole->known){
        ad_sub(c->origin,hole->center,delta);ad_cross(delta,hole->axis,cross);
        if(fabs(fabs(ad_dot(c->e3,hole->axis))-1)>1e-6||sqrt(ad_dot(cross,cross))>1e-5){e=PRO_TK_GENERAL_ERROR;goto done;}
    }else{memcpy(hole->center,c->origin,sizeof(ProVector));memcpy(hole->axis,c->e3,sizeof(ProVector));hole->known=1;}
    hole->walls++;
done:ProGeomitemdataFree(&raw);return e;
}
static ProError am_verify_hole(FdTxn *tx,AmHole *hole){
    ProError e=ProFeatureGeomitemVisit(&tx->feature,PRO_SURFACE,am_hole_wall,NULL,hole);
    return e?e:hole->walls?0:PRO_TK_E_NOT_FOUND;
}
