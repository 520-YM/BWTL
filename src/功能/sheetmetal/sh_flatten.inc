static ProError sh_flatten(FdTxn *tx,const wchar_t *fixed,int back){
    FdRef face={0};ProElement group;ProError e;
    if((e=md_solid_face(tx,fixed,&face))||(e=model_element(tx->root,back?PRO_E_SMT_BEND_BACK_SUB_TYPE:PRO_E_SMT_UNBEND_SUB_TYPE,1,0,NULL))||
       (e=model_element(tx->root,PRO_E_SMT_PRIMARY_FIXED_GEOM,0,0,&group))||(e=fd_ref_element(tx,group,PRO_E_SMT_FIXED_REF,&face))||
       (e=model_element(group,PRO_E_SMT_FIXED_REF_SIDE,1,PRO_SMT_FIXED_SIDE_UNDEF,NULL)))goto done;
    if(!back&&(e=model_element(tx->root,PRO_E_SMT_FLATTEN_FORM_WALLS,1,PRO_B_FALSE,NULL)))goto done;e=fd_create(tx,0);
done:fd_ref_free(&face);return e;
}
