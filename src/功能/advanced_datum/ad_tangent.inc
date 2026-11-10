static ProError ad_tangent(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProElement constraints,AdGeometry *g){
    ProSurface surface;
    ProSrftype type;
    ProUvParam uv;
    ProUvStatus inside;
    ProVector on_surface,deriv1[2],deriv2[3];
    ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&ref[0]))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_POINT,&ref[1]))||
       (e=ad_point(&ref[1],g->point))||(e=ProGeomitemToSurface(&ref[0].item,&surface))||(e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=PRO_SRF_CYL&&type!=PRO_SRF_CONE)return PRO_TK_UNSUPPORTED;
    if((e=ProSurfaceParamEval((ProSolid)tx->model,surface,g->point,uv))||
       (e=ProSurfaceUvpntVerify((ProSolid)tx->model,surface,uv,&inside))||
       (e=ProSurfaceXyzdataEval(surface,uv,on_surface,deriv1,deriv2,g->normal)))return e;
    if(inside!=PRO_UV_INSIDE||!ad_near(g->point,on_surface,1e-5*tx->scale)||!ad_unit(g->normal))return PRO_TK_BAD_INPUTS;
    if((e=ad_plane_constraint(tx,constraints,PRO_DTMPLN_TANG,&ref[0],0)))return e;
    return ad_plane_constraint(tx,constraints,PRO_DTMPLN_THRU,&ref[1],0);
}
