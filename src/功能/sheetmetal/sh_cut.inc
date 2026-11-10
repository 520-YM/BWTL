static ProError sh_cut(FdTxn *tx,const PartCommand *cmd){
    FdRef sketch={0};ProSectionShape shape;ProElement section,body,depth,side;ProError e;
#define CT(call) do{if((e=(call)))goto done;}while(0)
    CT(sd_sketch_ref(tx->model,cmd->arg[3]+7,&sketch,&shape));
    CT(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_EXTRUDE,NULL));
    CT(model_element(tx->root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,PRO_EXT_FEAT_TYPE_SOLID,NULL));
    CT(model_element(tx->root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_EXT_FEAT_FORM_NO_THIN,NULL));
    CT(model_element(tx->root,PRO_E_REMOVE_MATERIAL,1,PRO_EXT_MATERIAL_REMOVE,NULL));
    CT(model_element(tx->root,PRO_E_IS_SMT_CUT,1,PRO_B_TRUE,NULL));
    CT(model_element(tx->root,PRO_E_SMT_CUT_NORMAL_DIR,1,PRO_SMT_CUT_DRVSIDE_BOTH,NULL));
    CT(model_element(tx->root,PRO_E_STD_DIRECTION,1,!wcscmp(cmd->arg[4],L"POS")?PRO_EXT_CR_IN_SIDE_ONE:PRO_EXT_CR_IN_SIDE_TWO,NULL));
    CT(model_element(tx->root,PRO_E_BODY,0,0,&body));CT(model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL));CT(sd_existing_body(tx,body));
    CT(model_element(tx->root,PRO_E_STD_SECTION,0,0,&section));CT(fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&sketch));
    CT(model_element(tx->root,PRO_E_STD_EXT_DEPTH,0,0,&depth));CT(model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&side));
    CT(model_element(side,PRO_E_EXT_DEPTH_FROM_TYPE,1,PRO_EXT_DEPTH_FROM_NONE,NULL));CT(model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&side));
    CT(model_element(side,PRO_E_EXT_DEPTH_TO_TYPE,1,PRO_EXT_DEPTH_TO_ALL,NULL));CT(fd_create(tx,0));
done:fd_ref_free(&sketch);return e;
#undef CT
}
