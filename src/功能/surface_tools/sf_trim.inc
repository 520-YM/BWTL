static ProError sf_trim(FdTxn *tx,const PartCommand *cmd){
    FdRef quilt={0},tool={0};
    SxPath path={0};
    ProElement collection;
    int curve=sx_curve(cmd->arg[4]);
    ProError e;
    if((e=sf_ref(tx->model,cmd->arg[3],&quilt))||
       (e=model_element(tx->root,PRO_E_FEATURE_FORM,1,curve?PRO_NOTYPE:PRO_USE_SURFS,NULL))||
       (e=model_element(tx->root,PRO_E_SRF_TRIM_TYPE,1,curve?PRO_SURF_TRIM_USE_CRV:PRO_SURF_TRIM_USE_QLT,NULL))||
       (e=model_element(tx->root,PRO_E_SURF_TRIM_TYPE,1,curve?PRO_SURF_TRIM_USE_CRV:PRO_SURF_TRIM_USE_QLT,NULL))||
       (e=fd_ref_element(tx,tx->root,PRO_E_TRIM_QUILT,&quilt))||
       (e=model_element(tx->root,PRO_E_MATERIAL_SIDE,1,!wcscmp(cmd->arg[5],L"POS")?PRO_TRIM_MATL_SIDE_ONE:PRO_TRIM_MATL_SIDE_TWO,NULL))||
       (e=model_element(tx->root,PRO_E_PRIMARY_QLTSIDE,1,PRO_TRIM_PRIM_QLT_SIDE_ONE,NULL)))goto done;
    if(curve){
        if((e=sx_path_resolve(tx->model,cmd->arg[4]+6,tx->scale,&path))||
           (e=model_element(tx->root,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collection))||
           (e=sx_curve_collection(collection,&path)))goto done;
        tx->parents[tx->parent_count++]=path.parent;
    }else{
        e=sf_quilt(cmd->arg[4])?sf_ref(tx->model,cmd->arg[4],&tool):fd_ref_resolve(tx->model,cmd->arg[4],PRO_SURFACE,&tool);
        if(e||(e=fd_ref_element(tx,tx->root,PRO_E_STD_USEQLT_QLT,&tool)))goto done;
        e=model_element(tx->root,PRO_E_KEEP_TRIM_SURF_OPT,1,PRO_KEEP_TRIM_SURF_OPT_YES,NULL);
    }
done:
    fd_ref_free(&quilt);fd_ref_free(&tool);sx_path_free(&path);
    return e;
}
