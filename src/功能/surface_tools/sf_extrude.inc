static ProError sf_extrude(FdTxn *tx,const PartCommand *cmd,int kind){
    FdRef sketch={0},axis={0};
    ProSectionShape shape;
    FdSketch open={0};
    ProElement section=NULL,depth=NULL,from=NULL,to=NULL,body=NULL;
    int revolve=kind==SF_REVOLVE,fill=kind==SF_FILL;
    int direction=revolve?6:5;
    ProError e;
    if((e=fd_sketch_open(tx->model,cmd->arg[3],&open,0))||
       (e=ProSectionShapeGet(open.section,&shape)))goto done;
    if(fill||kind==SF_BODY_EXTRUDE){
        if(shape!=PRO_SECSHAPE_1_CLOSED_LOOP){e=PRO_TK_BAD_INPUTS;goto done;}
    }else if(shape!=PRO_SECSHAPE_1_OPEN_LOOP&&shape!=PRO_SECSHAPE_1_CLOSED_LOOP){e=PRO_TK_BAD_INPUTS;goto done;}
    sketch.item=*(ProGeomitem *)&open.feature;sketch.owner_feature=open.feature.id;
    if((e=ProSelectionAlloc(NULL,(ProModelitem *)&open.feature,&sketch.selection))||
       (e=model_element(tx->root,PRO_E_FEATURE_FORM,1,fill?PRO_FLAT:revolve?PRO_REVOLVE:PRO_EXTRUDE,NULL))||
       (e=model_element(tx->root,PRO_E_STD_SECTION,0,0,&section))||
       (e=fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&sketch)))goto done;
    if(fill)goto done;
    if((e=model_element(tx->root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,kind==SF_BODY_EXTRUDE?PRO_EXT_FEAT_TYPE_SOLID:PRO_EXT_FEAT_TYPE_SURFACE,NULL))||
       (e=model_element(tx->root,PRO_E_REMOVE_MATERIAL,1,PRO_EXT_MATERIAL_ADD,NULL))||
       (e=model_element(tx->root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_EXT_FEAT_FORM_NO_THIN,NULL))||
       (e=model_element(tx->root,PRO_E_STD_DIRECTION,1,!wcscmp(cmd->arg[direction],L"POS")?PRO_EXT_CR_IN_SIDE_ONE:PRO_EXT_CR_IN_SIDE_TWO,NULL)))goto done;
    if(kind==SF_BODY_EXTRUDE){
        if((e=model_element(tx->root,PRO_E_BODY,0,0,&body))||
           (e=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_NEW,NULL)))goto done;
    }
    if(revolve){
        if((e=fd_ref_resolve(tx->model,cmd->arg[4],PRO_AXIS,&axis))||
           (e=model_element(tx->root,PRO_E_REVOLVE_AXIS_OPT,1,PRO_REV_AXIS_EXT_REF,NULL))||
           (e=fd_ref_element(tx,tx->root,PRO_E_REVOLVE_AXIS,&axis))||
           (e=model_element(tx->root,PRO_E_REV_ANGLE,0,0,&depth))||
           (e=model_element(depth,PRO_E_REV_ANGLE_FROM,0,0,&from))||
           (e=model_element(from,PRO_E_REV_ANGLE_FROM_TYPE,1,PRO_REV_ANG_FROM_NONE,NULL))||
           (e=model_element(depth,PRO_E_REV_ANGLE_TO,0,0,&to))||
           (e=model_element(to,PRO_E_REV_ANGLE_TO_TYPE,1,PRO_REV_ANG_TO_ANGLE,NULL))||
           (e=model_element(to,PRO_E_REV_ANGLE_TO_VAL,2,as_arg(cmd,5),NULL)))goto done;
    }else{
        if((e=model_element(tx->root,PRO_E_STD_EXT_DEPTH,0,0,&depth))||
           (e=model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&from))||
           (e=model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,PRO_EXT_DEPTH_FROM_NONE,NULL))||
           (e=model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&to))||
           (e=model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,PRO_EXT_DEPTH_TO_BLIND,NULL))||
           (e=model_element(to,PRO_E_EXT_DEPTH_TO_VALUE,2,as_arg(cmd,4)*tx->scale,NULL)))goto done;
    }
done:
    fd_sketch_free(&open);fd_ref_free(&sketch);fd_ref_free(&axis);
    return e;
}
