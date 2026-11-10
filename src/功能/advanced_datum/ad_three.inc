static ProError ad_three(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProElement constraints,AdGeometry *g){
    ProVector xyz[3],a,b;
    int i;
    ProError e;
    for(i=0;i<3;i++)if((e=fd_ref_resolve(tx->model,cmd->arg[3+i],PRO_POINT,&ref[i]))||(e=ad_point(&ref[i],xyz[i])))return e;
    ad_sub(xyz[1],xyz[0],a);ad_sub(xyz[2],xyz[0],b);
    if(!ad_unit(a)||!ad_unit(b))return PRO_TK_BAD_INPUTS;
    ad_cross(a,b,g->normal);
    if(sqrt(ad_dot(g->normal,g->normal))<1e-7||!ad_unit(g->normal))return PRO_TK_BAD_INPUTS;
    memcpy(g->point,xyz[0],sizeof(g->point));
    for(i=0;i<3;i++)if((e=ad_plane_constraint(tx,constraints,PRO_DTMPLN_THRU,&ref[i],0)))return e;
    return PRO_TK_NO_ERROR;
}
