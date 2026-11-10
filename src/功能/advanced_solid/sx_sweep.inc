static ProError sx_sweep_build(FdTxn *tx,const SxPath *trajectory,const SxProfile *shape,int cut){
    ProElement frame=NULL,trajectories=NULL,origin=NULL,selection=NULL,chain=NULL,setup=NULL;
    ProElement profile=NULL,extracted=NULL,sketch_element=NULL,body=NULL;
    ProElempath path=NULL;
    ProElempathItem sketch_path[]={
        {PRO_ELEM_PATH_ITEM_TYPE_ID,{PRO_E_SWEEP_PROF_COMP}},
        {PRO_ELEM_PATH_ITEM_TYPE_ID,{PRO_E_SWEEP_SECTION}},
        {PRO_ELEM_PATH_ITEM_TYPE_ID,{PRO_E_SKETCHER}}
    };
    ProSection section=NULL;
    ProWSecerror errors=NULL;
    ProError e;
    int bodies;
#define SX_BUILD(stage_label,call) do{tx->stage=stage_label;e=(call);trace(stage_label,e);if(e)goto done;}while(0)
    if(cut){
        SX_BUILD("read sweep cut body count",sd_body_count(tx->model,&bodies));
        if(bodies!=1){e=PRO_TK_BAD_CONTEXT;goto done;}
        SX_BUILD("sweep cut body",model_element(tx->root,PRO_E_BODY,0,0,&body));
        SX_BUILD("sweep cut uses sole body",model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_ALL,NULL));
    }else SX_BUILD("target sweep body",sd_profile_body(tx,&bodies));
    SX_BUILD("sweep form",model_element(tx->root,PRO_E_FEATURE_FORM,1,PRO_SWEEP,NULL));
    SX_BUILD("sweep trajectory type",model_element(tx->root,PRO_E_SWEEP_TYPE,1,PRO_SWEEP_TYPE_MULTI_TRAJ,NULL));
    SX_BUILD("solid sweep",model_element(tx->root,PRO_E_EXT_SURF_CUT_SOLID_TYPE,1,PRO_SWEEP_FEAT_TYPE_SOLID,NULL));
    SX_BUILD("sweep material mode",model_element(tx->root,PRO_E_REMOVE_MATERIAL,1,cut?PRO_SWEEP_MATERIAL_REMOVE:PRO_SWEEP_MATERIAL_ADD,NULL));
    SX_BUILD("sweep full thickness",model_element(tx->root,PRO_E_FEAT_FORM_IS_THIN,1,PRO_SWEEP_FEAT_FORM_NO_THIN,NULL));
    if(cut)SX_BUILD("remove material inside sweep section",model_element(tx->root,PRO_E_STD_MATRLSIDE,1,PRO_SWEEP_MATERIAL_SIDE_TWO,NULL));
    SX_BUILD("sweep frame",model_element(tx->root,PRO_E_SWEEP_FRAME_COMP,0,0,&frame));
    SX_BUILD("sweep trajectory array",model_element(frame,PRO_E_FRM_OPT_TRAJ,0,0,&trajectories));
    SX_BUILD("sweep origin",model_element(trajectories,PRO_E_OPT_TRAJ,0,0,&origin));
    SX_BUILD("select sweep section path",model_element(origin,PRO_E_STD_SEC_METHOD,1,PRO_SEC_SELECT,NULL));
    SX_BUILD("select trajectory",model_element(origin,PRO_E_STD_SEC_SELECT,0,0,&selection));
    SX_BUILD("sweep trajectory chain",model_element(selection,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&chain));
    SX_BUILD("attach trajectory geometry",sx_curve_collection(chain,trajectory));
    SX_BUILD("sweep frame setup",model_element(frame,PRO_E_FRAME_SETUP,0,0,&setup));
    SX_BUILD("sweep normal to origin",model_element(setup,PRO_E_FRM_NORMAL,1,PRO_FRAME_NORM_ORIGIN,NULL));
    SX_BUILD("sweep automatic orientation",model_element(setup,PRO_E_FRM_ORIENT,1,PRO_FRAME_MIN,NULL));
    SX_BUILD("sweep automatic X",model_element(setup,PRO_E_FRM_USER_X,1,PRO_FRAME_DEFAULT_START_X,NULL));
    SX_BUILD("sweep profile",model_element(tx->root,PRO_E_SWEEP_PROF_COMP,0,0,&profile));
    SX_BUILD("sweep constant section mode",model_element(profile,PRO_E_SWP_SEC_TYPE,1,PRO_SWEEP_CONST_SECTION,NULL));
    SX_BUILD("create incomplete sweep",fd_create(tx,1));
    SX_BUILD("extract native sweep sketch",ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&extracted));
    SX_BUILD("allocate sweep sketch path",ProElempathAlloc(&path));
    SX_BUILD("set sweep sketch path",ProElempathDataSet(path,sketch_path,_countof(sketch_path)));
    SX_BUILD("get sweep sketch element",ProElemtreeElementGet(extracted,path,&sketch_element));
    SX_BUILD("get sweep section",ProElementSpecialvalueGet(sketch_element,NULL,(ProAppData *)&section));
    if(!section){e=PRO_TK_GENERAL_ERROR;goto done;}
    SX_BUILD("initialize sweep solver",ProSectionIntentManagerModeSet(section,PRO_B_FALSE));
    SX_BUILD("draw requested sweep section",sx_sweep_draw(tx->model,section,shape));
    SX_BUILD("allocate sweep section diagnostics",ProSecerrorAlloc(&errors));
    SX_BUILD("solve sweep section",ProSectionAutodim(section,&errors));
    SX_BUILD("regenerate sweep section",ProSectionRegenerate(section,&errors));
    SX_BUILD("update sweep sketch",ProElementSpecialvalueSet(sketch_element,(ProAppData)section));
    tx->options[0]=PRO_FEAT_CR_NO_OPTS;
    SX_BUILD("complete native sweep",ProFeatureWithoptionsRedefine(NULL,&tx->feature,extracted,tx->options,
        PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx->errors));
done:
    if(errors)ProSecerrorFree(&errors);
    if(path)ProElempathFree(&path);
    if(extracted)ProFeatureElemtreeFree(&tx->feature,extracted);
    return e;
#undef SX_BUILD
}

static void sx_path_free(SxPath *path){for(int i=0;i<path->count;i++)fd_ref_free(&path->items[i]);}

static ProError sx_sweep(const PartCommand *cmd,ProMdl model,wchar_t *report,size_t capacity){
    FdTxn tx={0};
    SxPath path={0};
    SxProfile section={L"CIRCLE",NULL,0,0};
    int kind=sx_kind(cmd->arg[0]),cut=kind==SX_CUT,bodies=0;
    double volume=0;
    ProError e;
#define SX_RUN(label,call) do{tx.stage=label;e=(call);trace(label,e);if(e)goto done;}while(0)
    SX_RUN("validate sweep",fd_begin(&tx,model,cmd->arg[2],cut?PRO_FEAT_CUT:PRO_FEAT_PROTRUSION));
    SX_RUN("read initial sweep body count",sd_body_count(model,&bodies));
    if(!kind){
        FdRef *ref=&path.items[0];
        ProCurve curve;
        ProEnttype type;
        path.count=1;
        SX_RUN("resolve legacy sweep trajectory",sd_profile_curve(model,cmd->arg[3]+6,ref));
        SX_RUN("initialize legacy trajectory",ProGeomitemToCurve(&ref->item,&curve));
        SX_RUN("read legacy trajectory type",ProCurveTypeGet(curve,&type));
        if(type!=PRO_ENT_LINE&&type!=PRO_ENT_ARC){e=PRO_TK_BAD_INPUTS;goto done;}
        path.parent=ref->owner_feature;
    }else SX_RUN("resolve open continuous sweep path",sx_path_resolve(model,cmd->arg[3]+6,tx.scale,&path));
    if(kind==SX_PROFILE||kind==SX_CUT)section=sx_profile_command(cmd,4,tx.scale);
    else section.first=as_arg(cmd,4)*tx.scale;
    if((e=sx_sweep_build(&tx,&path,&section,cut)))goto done;
    SX_RUN("verify sweep volume and path parent",sd_profile_verify_change(&tx,PRO_SWEEP,&path.parent,1,cut,&volume));
    if(!kind)swprintf_s(report,capacity,L"已创建恒定圆截面实体扫描 %ls，路径 %ls，直径 %.9g mm。特征 ID %d，零件体积 %.9g mm³。路径关联与重生成已核对，未保存。",
        cmd->arg[2],cmd->arg[3]+6,section.first/tx.scale,tx.feature.id,volume);
    else swprintf_s(report,capacity,L"已创建%ls %ls，路径 %ls（%d 段）。特征 ID %d，零件体积 %.9g mm³。已核对路径关联和重生成，未保存。",
        cut?L"扫描切除":L"实体扫描",cmd->arg[2],cmd->arg[3]+6,path.count,tx.feature.id,volume);
done:
    sx_path_free(&path);
    sd_profile_finish(&tx,e,bodies,report,capacity);
    return e;
#undef SX_RUN
}
