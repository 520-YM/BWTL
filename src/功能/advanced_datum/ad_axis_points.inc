static ProError ad_axis_points(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],AdGeometry *g){
    ProElement array=NULL,entry=NULL;
    ProVector end;
    int i;
    ProError e;
    if((e=fd_ref_resolve(tx->model,cmd->arg[3],PRO_POINT,&ref[0]))||
       (e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_POINT,&ref[1]))||
       (e=ad_point(&ref[0],g->point))||(e=ad_point(&ref[1],end)))return e;
    ad_sub(end,g->point,g->normal);
    if(!ad_unit(g->normal))return PRO_TK_BAD_INPUTS;
    if((e=model_element(tx->root,PRO_E_DTMAXIS_CONSTRAINTS,0,0,&array)))return e;
    for(i=0;i<2;i++)if((e=model_element(array,PRO_E_DTMAXIS_CONSTRAINT,0,0,&entry))||
       (e=model_element(entry,PRO_E_DTMAXIS_CONSTR_TYPE,1,PRO_DTMAXIS_CONSTR_TYPE_THRU,NULL))||
       (e=fd_ref_element(tx,entry,PRO_E_DTMAXIS_CONSTR_REF,&ref[i])))return e;
    return PRO_TK_NO_ERROR;
}
