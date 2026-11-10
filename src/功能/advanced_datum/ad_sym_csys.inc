static ProError ad_sym_csys(FdTxn *tx,const PartCommand *cmd,FdRef ref[3],AdGeometry *g){
    ProVector origins[3],directions[3];
    ProElement constraints=NULL,entry=NULL;
    int i,j,x_negative=!wcscmp(cmd->arg[6],L"NEG"),y_negative=!wcscmp(cmd->arg[7],L"NEG");
    ProError e;
    for(i=0;i<3;i++)if((e=fd_ref_resolve(tx->model,cmd->arg[3+i],PRO_SURFACE,&ref[i]))||
       (e=fd_plane_data(&ref[i].item,origins[i],directions[i])))return e;
    for(i=0;i<3;i++)for(j=i+1;j<3;j++)if(fabs(ad_dot(directions[i],directions[j]))>1e-7)return PRO_TK_BAD_INPUTS;
    for(i=0;i<3;i++){
        double offset=ad_dot(origins[i],directions[i]);
        for(j=0;j<3;j++)g->point[j]+=offset*directions[i][j];
        g->normal[i]=(x_negative?-1:1)*directions[0][i];
        g->axis_y[i]=(y_negative?-1:1)*directions[1][i];
    }
    if((e=model_element(tx->root,PRO_E_CSYS_ORIGIN_CONSTRS,0,0,&constraints)))return e;
    for(i=0;i<3;i++)if((e=model_element(constraints,PRO_E_CSYS_ORIGIN_CONSTR,0,0,&entry))||
       (e=fd_ref_element(tx,entry,PRO_E_CSYS_ORIGIN_CONSTR_REF,&ref[i])))return e;
    if((e=model_element(tx->root,PRO_E_CSYS_ORIENT_BY_METHOD,1,PRO_CSYS_ORIENT_BY_SEL_REFS,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_CSYS_ORIENTSELAXIS1_REF,&ref[0]))||
       (e=model_element(tx->root,PRO_E_CSYS_ORIENTSELAXIS1_OPT,1,PRO_CSYS_ORIENTMOVE_AXIS_OPT_X,NULL))||
       (e=model_element(tx->root,PRO_E_CSYS_ORIENTSELAXIS1_FLIP,1,x_negative?PRO_CSYS_ORIENTSELAXIS_FLIP_YES:PRO_CSYS_ORIENTSELAXIS_FLIP_NO,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_CSYS_ORIENTSELAXIS2_REF,&ref[1]))||
       (e=model_element(tx->root,PRO_E_CSYS_ORIENTSELAXIS2_OPT,1,PRO_CSYS_ORIENTMOVE_AXIS_OPT_Y,NULL)))return e;
    return model_element(tx->root,PRO_E_CSYS_ORIENTSELAXIS2_FLIP,1,y_negative?PRO_CSYS_ORIENTSELAXIS_FLIP_YES:PRO_CSYS_ORIENTSELAXIS_FLIP_NO,NULL);
}
