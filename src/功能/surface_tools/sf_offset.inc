static ProError sf_offset(FdTxn *tx,const PartCommand *cmd){
    FdRef quilt={0};
    ProError e;
    if((e=sf_ref(tx->model,cmd->arg[3],&quilt))||
       /* Native Creo 10 offset seed records datum-surface subtype 16. */
       (e=model_element(tx->root,PRO_E_DTM_SRF_TYPE,1,16,NULL))||
       (e=model_element(tx->root,PRO_E_OFFSET_FEATURE_TYPE,1,1,NULL))||
       (e=model_element(tx->root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,0,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_SRF_OFFS_REFQUILT,&quilt))||
       (e=model_element(tx->root,PRO_E_SRF_OFFS_METHOD,1,PRO_OFFS_METH_NORMTOSURF,NULL))||
       (e=model_element(tx->root,PRO_E_SRF_OFFS_DIRECTION,1,as_arg(cmd,4)>0?-1:1,NULL)))goto done;
    e=model_element(tx->root,PRO_E_SRF_OFFS_OFFSET,2,fabs(as_arg(cmd,4))*tx->scale,NULL);
done:
    fd_ref_free(&quilt);
    return e;
}
