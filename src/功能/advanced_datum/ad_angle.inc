static ProError ad_angle(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProElement constraints,AdGeometry *g){
    ProVector plane_point,plane_normal,direction,distance,cross;
    double angle,radians;
    int i;
    ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_SURFACE,&ref[0]))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],ad_ref(cmd->arg[4],L"AXIS:")?PRO_AXIS:PRO_EDGE,&ref[1]))||
       (e=fd_plane_data(&ref[0].item,plane_point,plane_normal))||(e=ad_line(&ref[1],g->point,direction)))return e;
    ad_sub(g->point,plane_point,distance);
    if(fabs(ad_dot(distance,plane_normal))>1e-5*tx->scale||fabs(ad_dot(direction,plane_normal))>1e-7)return PRO_TK_BAD_INPUTS;
    part_number(cmd->arg[5],&angle);
    radians=angle*3.141592653589793/180;
    ad_cross(direction,plane_normal,cross);
    for(i=0;i<3;i++)g->normal[i]=cos(radians)*plane_normal[i]+sin(radians)*cross[i];
    if((e=ad_plane_constraint(tx,constraints,PRO_DTMPLN_THRU,&ref[1],0)))return e;
    return ad_plane_constraint(tx,constraints,PRO_DTMPLN_ANG,&ref[0],angle);
}
