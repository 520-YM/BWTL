static ProError am_radial(FdTxn *tx,const PartCommand *cmd,AmHole *geometry){
    FdRef face={0},angle={0},axial={0};ProSurface surf;ProSrftype type;
    ProElement placement;ProVector p,n,q,m;ProGeomitemdata *raw=NULL;ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&face))||
       (e=ProGeomitemToSurface(&face.item,&surf))||(e=ProSurfaceTypeGet(surf,&type)))goto done;
    if(type!=PRO_SRF_CYL&&type!=PRO_SRF_CONE){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&angle))||
       (e=fd_ref_resolve(tx->model,cmd->arg[6],PRO_SURFACE,&axial))||
       (e=fd_plane_data(&angle.item,p,n))||(e=fd_plane_data(&axial.item,q,m))||
       (e=ProSurfaceDataGet(surf,&raw)))goto done;
    {double *axis=type==PRO_SRF_CYL?raw->data.p_surface_data->srf_shape.cylinder.e3:raw->data.p_surface_data->srf_shape.cone.e3;
     if(fabs(ad_dot(n,axis))>1e-6||fabs(fabs(ad_dot(m,axis))-1)>1e-6){e=PRO_TK_BAD_INPUTS;goto done;}}
    geometry->radius=as_arg(cmd,8)*tx->scale/2;
    if((e=am_flat_hole(tx,2*geometry->radius,as_arg(cmd,10)*tx->scale))||
       (e=model_element(tx->root,PRO_E_HLE_PLACEMENT,0,0,&placement))||
       (e=fd_ref_element(tx,placement,PRO_E_HLE_PRIM_REF,&face))||
       (e=model_element(placement,PRO_E_HLE_PL_TYPE,1,PRO_HLE_PL_TYPE_RAD,NULL))||
       (e=fd_ref_element(tx,placement,PRO_E_HLE_REF_PLANE,&angle))||
       (e=model_element(placement,PRO_E_HLE_REF_ANG,2,as_arg(cmd,5),NULL))||
       (e=fd_ref_element(tx,placement,PRO_E_HLE_NORM_PLA,&axial))||
       (e=model_element(placement,PRO_E_HLE_NORM_OFFST,2,as_arg(cmd,7)*tx->scale,NULL)))goto done;
    e=fd_create(tx,0);
done:
    if(raw)ProGeomitemdataFree(&raw);fd_ref_free(&face);fd_ref_free(&angle);fd_ref_free(&axial);return e;
}
