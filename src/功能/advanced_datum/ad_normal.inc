static ProError ad_normal(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],ProElement constraints,AdGeometry *g){
    ProVector on_curve;
    double parameter;
    ProError e;
    if((e=ad_curve_ref(tx->model,cmd->arg[3],&ref[0],&g->curve))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_POINT,&ref[1]))||(e=ad_point(&ref[1],g->point))||
       (e=ProCurveParamEval(g->curve,g->point,&parameter))||
       (e=ProCurveXyzdataEval(g->curve,parameter,on_curve,g->normal,NULL)))return e;
    if(!ad_near(g->point,on_curve,1e-5*tx->scale)||!ad_unit(g->normal))return PRO_TK_BAD_INPUTS;
    if((e=ad_plane_constraint(tx,constraints,PRO_DTMPLN_NORM,&ref[0],0)))return e;
    return ad_plane_constraint(tx,constraints,PRO_DTMPLN_THRU,&ref[1],0);
}
