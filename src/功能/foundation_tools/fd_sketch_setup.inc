static ProError fd_sketch_setup(FdTxn *tx,const wchar_t *plane,const wchar_t *orientation){
    FdRef refs[2]={{0}};ProElement section=NULL,setup=NULL;ProVector p,n,q,m;ProError e;double dot;
    e=fd_ref_resolve(tx->model,plane,PRO_SURFACE,&refs[0]);if(e)goto done;
    e=fd_ref_resolve(tx->model,orientation,PRO_SURFACE,&refs[1]);if(e)goto done;
    if((e=fd_plane_data(&refs[0].item,p,n))||(e=fd_plane_data(&refs[1].item,q,m)))goto done;
    dot=n[0]*m[0]+n[1]*m[1]+n[2]*m[2];if(fabs(dot)>1-1e-9){e=PRO_TK_BAD_INPUTS;goto done;}
    if((e=model_element(tx->root,PRO_E_CURVE_TYPE,1,PRO_CURVE_TYPE_SKETCHED,NULL))||
       (e=model_element(tx->root,PRO_E_STD_SECTION,0,0,&section))||
       (e=model_element(section,PRO_E_STD_SEC_SETUP_PLANE,0,0,&setup))||
       (e=fd_ref_element(tx,setup,PRO_E_STD_SEC_PLANE,&refs[0]))||
       (e=model_element(setup,PRO_E_STD_SEC_PLANE_VIEW_DIR,1,PRO_SEC_VIEW_DIR_SIDE_ONE,NULL))||
       (e=model_element(setup,PRO_E_STD_SEC_PLANE_ORIENT_DIR,1,PRO_SEC_ORIENT_DIR_RIGHT,NULL))||
       (e=fd_ref_element(tx,setup,PRO_E_STD_SEC_PLANE_ORIENT_REF,&refs[1])))goto done;
done:fd_ref_free(&refs[0]);fd_ref_free(&refs[1]);return e;
}
