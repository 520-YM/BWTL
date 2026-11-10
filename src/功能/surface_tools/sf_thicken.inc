static ProError sf_thicken(FdTxn *tx,const PartCommand *cmd,int solidify){
    FdRef quilt={0};
    ProElement body;
    ProError e;
    if((e=sf_ref(tx->model,cmd->arg[3],&quilt))||
       (e=model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_USE_SURFS,NULL)))goto done;
    if(solidify){
        if((e=fd_ref_element(tx,tx->root,PRO_E_PATCH_QUILT,&quilt))||
           (e=model_element(tx->root,PRO_E_PATCH_TYPE,1,PRO_FEAT_PROTRUSION,NULL)))goto done;
    }else{
        int side=!wcscmp(cmd->arg[5],L"SYM")?PRO_THICKEN_BOTH_SIDES:!wcscmp(cmd->arg[5],L"POS")?PRO_THICKEN_SIDE_ONE:PRO_THICKEN_SIDE_TWO;
        if((e=model_element(tx->root,PRO_E_FEAT_FORM_ALWAYS_THIN,1,PRO_THIN,NULL))||
           (e=fd_ref_element(tx,tx->root,PRO_E_STD_USEQLT_QLT,&quilt))||
           (e=model_element(tx->root,PRO_E_STD_USEQLT_SIDE,1,side,NULL))||
           (e=model_element(tx->root,PRO_E_THICKNESS,2,as_arg(cmd,4)*tx->scale,NULL))||
           (e=model_element(tx->root,PRO_E_SRF_OFFS_METHOD,1,PRO_OFFS_METH_NORMTOSURF,NULL)))goto done;
    }
    if((e=model_element(tx->root,PRO_E_BODY,0,0,&body))||
       (e=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_NEW,NULL)))goto done;
done:
    fd_ref_free(&quilt);
    return e;
}
