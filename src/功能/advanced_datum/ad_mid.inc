static ProError ad_mid(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProElement constraints,AdGeometry *g){
    ProVector p[2],normal,delta;
    int i;
    ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&ref[0]))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&ref[1]))||
       (e=fd_plane_data(&ref[0].item,p[0],g->normal))||(e=fd_plane_data(&ref[1].item,p[1],normal)))return e;
    ad_sub(p[1],p[0],delta);
    if(fabs(fabs(ad_dot(g->normal,normal))-1)>1e-7||fabs(ad_dot(delta,g->normal))<1e-5*tx->scale)return PRO_TK_BAD_INPUTS;
    for(i=0;i<3;i++)g->point[i]=(p[0][i]+p[1][i])/2;
    if((e=ad_plane_constraint(tx,constraints,PRO_DTMPLN_MIDPLN,&ref[0],0)))return e;
    return ad_plane_constraint(tx,constraints,PRO_DTMPLN_PRL,&ref[1],0);
}
