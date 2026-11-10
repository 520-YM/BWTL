static ProError sh_form(FdTxn *tx,const PartCommand *cmd){
    FdRef sketch={0};ProSectionShape shape;ProElement section,group;ProError e;
#define FM(call) do{tx->stage=#call;e=(call);trace(#call,e);if(e)goto done;}while(0)
    FM(sd_sketch_ref(tx->model,cmd->arg[3]+7,&sketch,&shape));
    /* Creo 10's ProSmtSketchForm.h omits this required inherited element.
       Verified against a successful native UI feature and its element tree. */
    FM(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_EXTRUDE,NULL));
    FM(model_element(tx->root,PRO_E_STD_SECTION,0,0,&section));FM(fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&sketch));
    FM(model_element(tx->root,PRO_E_SKETCH_FORM_TYPE,1,PRO_SMT_SKETCH_FORM_TYPE_PUNCH,NULL));
    FM(model_element(tx->root,PRO_E_SKETCH_FORM_DEPTH_PUNCH,2,as_arg(cmd,4)*tx->scale,NULL));
    FM(model_element(tx->root,PRO_E_SKETCH_FORM_DIR,1,!wcscmp(cmd->arg[6],L"NEG"),NULL));
    FM(model_element(tx->root,PRO_E_EXT_COMP_DRFT_ANG,0,0,&group));FM(model_element(group,PRO_E_EXT_DRFT_ANG,1,PRO_EXT_DRFT_ANG_NO_DRAFT,NULL));
    FM(model_element(tx->root,PRO_E_SRF_END_ATTRIBUTES,1,PRO_EXT_SURF_END_ATTR_CAPPED,NULL));
    FM(model_element(tx->root,PRO_E_FORM_PUNCH_MATERIAL_SIDE,1,PRO_SMT_PUNCH_MAT_OUTSIDE,NULL));
    for(int i=0;i<2;i++){
        FM(model_element(tx->root,i?PRO_E_SMT_FILLET_QUILT:PRO_E_SMT_FILLET_INTERSECT,0,0,&group));
        FM(model_element(group,PRO_E_SMT_FILLET_RADIUS_USEFLAG,1,PRO_B_TRUE,NULL));
        FM(model_element(group,PRO_E_SMT_FILLET_RADIUS_SIDE,1,PRO_BEND_RAD_INSIDE,NULL));
        FM(model_element(group,PRO_E_SMT_FILLET_RADIUS_VALUE,2,as_arg(cmd,5)*tx->scale,NULL));
    }
    FM(model_element(tx->root,PRO_E_SMT_TRIM_FORM_SIDES,1,PRO_B_FALSE,NULL));FM(fd_create(tx,0));
done:fd_ref_free(&sketch);return e;
#undef FM
}
