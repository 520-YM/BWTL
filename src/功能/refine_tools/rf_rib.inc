static ProError rf_rib(FdTxn *tx,const PartCommand *cmd){
    FdSketch sketch={0};
    FdRef ref={0};
    ProSectionShape shape;
    ProElement body=NULL,compound=NULL,section=NULL;
    int section_side=!wcscmp(cmd->arg[5],L"POS")?PRO_RIB_SEC_SIDE_ONE:PRO_RIB_SEC_SIDE_TWO;
    int width_side=!wcscmp(cmd->arg[6],L"SYM")?PRO_RIB_SYMMETRIC:!wcscmp(cmd->arg[6],L"POS")?PRO_RIB_SIDE_ONE:PRO_RIB_SIDE_TWO;
    double width;
    ProError e;
#define RIB_STEP(call) do{e=(call);if(e)goto done;}while(0)
    part_number(cmd->arg[4],&width);width*=tx->scale;
    tx->stage="resolve one open associative rib sketch";
    RIB_STEP(fd_sketch_open(tx->model,cmd->arg[3],&sketch,0));
    RIB_STEP(ProSectionShapeGet(sketch.section,&shape));
    if(shape!=PRO_SECSHAPE_1_OPEN_LOOP){e=PRO_TK_BAD_INPUTS;goto done;}
    ref.item=*(ProGeomitem *)&sketch.feature;ref.owner_feature=sketch.feature.id;
    RIB_STEP(ProSelectionAlloc(NULL,(ProModelitem *)&ref.item,&ref.selection));
    tx->stage="build native profile rib";
    RIB_STEP(model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_EXTRUDE,NULL));
    RIB_STEP(model_element(tx->root,PRO_E_BODY,0,0,&body));
    RIB_STEP(model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL));
    RIB_STEP(sd_existing_body(tx,body));
    RIB_STEP(model_element(tx->root,PRO_E_RIB_SECTION_COMP,0,0,&compound));
    RIB_STEP(model_element(compound,PRO_E_STD_SECTION,0,0,&section));
    RIB_STEP(fd_ref_element(tx,section,PRO_E_SEC_USE_SKETCH,&ref));
    RIB_STEP(model_element(compound,PRO_E_STD_MATRLSIDE,1,section_side,NULL));
    RIB_STEP(model_element(tx->root,PRO_E_RIB_THICKNESS,2,width,NULL));
    RIB_STEP(model_element(tx->root,PRO_E_RIB_SIDE_OPTS,1,width_side,NULL));
    tx->stage="create and complete native profile rib";
    RIB_STEP(fd_create(tx,1));
    {
        ProElement extracted=NULL;
        e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&extracted);
        if(!e){
            tx->options[0]=PRO_FEAT_CR_NO_OPTS;
            e=ProFeatureWithoptionsRedefine(NULL,&tx->feature,extracted,tx->options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->errors);
        }
        if(extracted)ProFeatureElemtreeFree(&tx->feature,extracted);
        if(e)goto done;
    }
    tx->stage="verify native rib thickness and both sides";
    {
        const ProElemId thick[]={PRO_E_RIB_THICKNESS},side[]={PRO_E_RIB_SIDE_OPTS};
        const ProElemId material[]={PRO_E_RIB_SECTION_COMP,PRO_E_STD_MATRLSIDE};
        RIB_STEP(mc_tree_value(&tx->feature,thick,1,width,0));
        RIB_STEP(mc_tree_value(&tx->feature,side,1,width_side,1));
        RIB_STEP(mc_tree_value(&tx->feature,material,2,section_side,1));
    }
done:
    fd_ref_free(&ref);fd_sketch_free(&sketch);
    return e;
#undef RIB_STEP
}
