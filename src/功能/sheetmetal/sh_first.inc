static ProError sh_first(FdTxn *tx,const PartCommand *cmd){
    FdRef sketch={0};ProSectionShape shape;ProElement section,body;ProError e;
#define FW(call) do{if((e=(call)))goto done;}while(0)
    FW(sd_sketch_ref(tx->model,cmd->arg[3]+7,&sketch,&shape));FW(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_FLAT,NULL));
    FW(model_element(tx->root,PRO_E_IS_UNATTACHED_WALL,1,PRO_B_TRUE,NULL));
    FW(model_element(tx->root,PRO_E_STD_DIRECTION,1,!wcscmp(cmd->arg[5],L"POS")?1:-1,NULL));
    FW(model_element(tx->root,PRO_E_STD_SMT_THICKNESS,2,as_arg(cmd,4)*tx->scale,NULL));FW(model_element(tx->root,PRO_E_STD_SMT_SWAP_DRV_SIDE,1,0,NULL));
    FW(model_element(tx->root,PRO_E_BODY,0,0,&body));FW(model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_NEW,NULL));
    FW(model_element(tx->root,PRO_E_SMT_NEW_BODY_LINKED,1,PRO_B_TRUE,NULL));
    FW(model_element(tx->root,PRO_E_STD_SECTION,0,0,&section));FW(fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&sketch));FW(fd_create(tx,0));
done:fd_ref_free(&sketch);return e;
#undef FW
}
