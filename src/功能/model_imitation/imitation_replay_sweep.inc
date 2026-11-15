typedef struct {
    ProElement chain,setup,profile;
    ProSection section;
    int cut,body_mode,section_mode;
} ReplaySweep;

static ProError replay_sweep_section(ProElement tree,ProElement *sketch,ProSection *section)
{
    const ProElemId path[]={PRO_E_SWEEP_PROF_COMP,PRO_E_SWEEP_SECTION,PRO_E_SKETCHER};
    ProError error=mc_tree_element(tree,(ProElemId *)path,_countof(path),sketch);
    if(!error)error=ProElementSpecialvalueGet(*sketch,NULL,(ProAppData *)section);
    return error?error:*section?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError replay_sweep_read(ProElement tree,ReplaySweep *sweep)
{
    ProElement frame=NULL,array=NULL,sketch=NULL;
    ProElement *trajectories=NULL;
    int count=0,value=0;
    ProError error=replay_tree_integer(tree,PRO_E_SWEEP_TYPE,&value);
    if(!error&&value!=PRO_SWEEP_TYPE_MULTI_TRAJ)error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SWEEP_FRAME_COMP},1,&frame);
    if(!error)error=mc_tree_element(frame,(ProElemId[]){PRO_E_FRM_OPT_TRAJ},1,&array);
    if(!error)error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&trajectories);
    if(!error)error=ProElementArrayGet(array,NULL,&trajectories);
    if(!error&&trajectories)error=ProArraySizeGet((ProArray)trajectories,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(trajectories[0],PRO_E_STD_SEC_METHOD,&value);
    if(!error&&value!=PRO_SEC_SELECT)error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(trajectories[0],(ProElemId[]){PRO_E_STD_SEC_SELECT,PRO_E_STD_CURVE_COLLECTION_APPL},2,&sweep->chain);
    if(!error)error=mc_tree_element(frame,(ProElemId[]){PRO_E_FRAME_SETUP},1,&sweep->setup);
    const struct {ProElemId id;int expected;} frame_settings[]={
        {PRO_E_FRM_NORMAL,PRO_FRAME_NORM_ORIGIN},{PRO_E_FRM_ORIENT,PRO_FRAME_MIN},
        {PRO_E_FRM_USER_X,PRO_FRAME_DEFAULT_START_X}
    };
    for(int i=0;!error&&i<_countof(frame_settings);i++){
        error=replay_tree_integer(sweep->setup,frame_settings[i].id,&value);
        if(!error&&value!=frame_settings[i].expected)error=PRO_TK_UNSUPPORTED;
    }
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SWEEP_PROF_COMP},1,&sweep->profile);
    if(!error)error=replay_tree_integer(sweep->profile,PRO_E_SWP_SEC_TYPE,&value);
    if(!error&&value!=PRO_SWEEP_CONST_SECTION&&value!=PRO_SWEEP_VARIABLE_SECTION)error=PRO_TK_UNSUPPORTED;
    if(!error)sweep->section_mode=value;
    if(!error)error=replay_tree_integer(tree,PRO_E_EXT_SURF_CUT_SOLID_TYPE,&value);
    if(!error&&value!=PRO_SWEEP_FEAT_TYPE_SOLID)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(tree,PRO_E_FEAT_FORM_IS_THIN,&value);
    if(!error&&value!=PRO_SWEEP_FEAT_FORM_NO_THIN)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_tree_integer(tree,PRO_E_REMOVE_MATERIAL,&value);
    if(!error&&value!=PRO_SWEEP_MATERIAL_ADD&&value!=PRO_SWEEP_MATERIAL_REMOVE)error=PRO_TK_UNSUPPORTED;
    if(!error)sweep->cut=value==PRO_SWEEP_MATERIAL_REMOVE;
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_BODY,PRO_E_BODY_USE},2,&array);
    if(!error)error=ProElementIntegerGet(array,NULL,&sweep->body_mode);
    if(!error&&!mc_tree_element(tree,(ProElemId[]){PRO_E_SWP_ATTR,PRO_E_END_SRF_ATTR},2,&array)){
        error=ProElementIntegerGet(array,NULL,&value);
        if(!error&&value)error=PRO_TK_UNSUPPORTED;
    }
    if(!error)error=replay_sweep_section(tree,&sketch,&sweep->section);
    if(trajectories)ProArrayFree((ProArray *)&trajectories);
    return error;
}

static ProError replay_sweep_tree(ImitationReplay *replay,ProElement original,const ReplaySweep *source,FdTxn *tx)
{
    ProElement frame=NULL,array=NULL,trajectory=NULL,selection=NULL,chain=NULL,setup=NULL,profile=NULL;
    int bodies=0;
    ProError error=sd_body_count(tx->model,&bodies);
    if(!error&&((source->cut&&(bodies!=1||source->body_mode!=PRO_BODY_USE_ALL))||
       (!source->cut&&((source->body_mode!=PRO_BODY_USE_SELECTED&&source->body_mode!=PRO_BODY_USE_NEW)||
                      (bodies==1&&source->body_mode!=PRO_BODY_USE_SELECTED)||bodies>1))))
        error=PRO_TK_UNSUPPORTED;
    if(!error&&!source->cut&&source->body_mode==PRO_BODY_USE_SELECTED){
        ProElement selection=NULL;
        ProReference reference=NULL;
        ProSolidBody original_body;
        ProSolidBodyState state;
        ProBoolean construction;
        int id=0,count=0;
        error=mc_tree_element(original,(ProElemId[]){PRO_E_BODY,PRO_E_BODY_SELECT},2,&selection);
        if(!error)error=ProElementReferenceGet(selection,NULL,&reference);
        if(!error)error=ProReferenceIdGet(reference,&id);
        if(!error)error=sd_body_count(replay->source,&count);
        if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProModelitemInit(replay->source,id,PRO_BODY,&original_body);
        if(!error)error=ProSolidBodyStateGet(&original_body,&state);
        if(!error)error=ProSolidBodyIsConstruction(&original_body,&construction);
        if(!error&&(state!=PRO_BODY_STATE_ACTIVE||construction))error=PRO_TK_UNSUPPORTED;
        if(reference)ProReferenceFree(reference);
    }
    if(!error&&source->cut){
        ProElement body=NULL;
        error=model_element(tx->root,PRO_E_BODY,0,0,&body);
        if(!error)error=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_ALL,NULL);
        if(!error)error=replay_pattern_field(replay,original,tx->root,PRO_E_STD_MATRLSIDE);
    }else if(!error)error=sd_profile_body(tx,&bodies);
    const ProElemId fields[]={PRO_E_FEATURE_FORM,PRO_E_SWEEP_TYPE,PRO_E_EXT_SURF_CUT_SOLID_TYPE,
        PRO_E_REMOVE_MATERIAL,PRO_E_FEAT_FORM_IS_THIN};
    for(int i=0;!error&&i<_countof(fields);i++)error=replay_pattern_field(replay,original,tx->root,fields[i]);
    if(!error)error=model_element(tx->root,PRO_E_SWEEP_FRAME_COMP,0,0,&frame);
    if(!error)error=model_element(frame,PRO_E_FRM_OPT_TRAJ,0,0,&array);
    if(!error)error=model_element(array,PRO_E_OPT_TRAJ,0,0,&trajectory);
    if(!error)error=model_element(trajectory,PRO_E_STD_SEC_METHOD,1,PRO_SEC_SELECT,NULL);
    if(!error)error=model_element(trajectory,PRO_E_STD_SEC_SELECT,0,0,&selection);
    if(!error)error=model_element(selection,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&chain);
    if(!error)error=replay_curve_collection(replay,source->chain,chain);
    if(!error)error=model_element(frame,PRO_E_FRAME_SETUP,0,0,&setup);
    const ProElemId frame_fields[]={PRO_E_FRM_NORMAL,PRO_E_FRM_ORIENT,PRO_E_FRM_USER_X};
    for(int i=0;!error&&i<_countof(frame_fields);i++)error=replay_pattern_field(replay,source->setup,setup,frame_fields[i]);
    if(!error)error=model_element(tx->root,PRO_E_SWEEP_PROF_COMP,0,0,&profile);
    if(!error)error=replay_pattern_field(replay,source->profile,profile,PRO_E_SWP_SEC_TYPE);
    return error;
}

static ProError replay_sweep(ImitationReplay *replay,ProFeature *original_feature,ProElement original,ProFeattype type,ProName label,
                             ProFeature *created,int *entities,int *dimensions)
{
    FdTxn tx={0};
    ReplaySweep source={0};
    ProElement extracted=NULL,sketch=NULL;
    ProSection section=NULL;
    const char *stage="read native sweep";
    ProError error=replay_sweep_read(original,&source);
    if(!error&&source.section_mode==PRO_SWEEP_VARIABLE_SECTION){
        ProRelset relations;
        ProError lookup=ProModelitemToRelset(original_feature,&relations);
        if(!lookup){
            ProWstring *lines=NULL;
            int count=0;
            error=ProArrayAlloc(0,sizeof(*lines),4,(ProArray *)&lines);
            if(!error)error=ProRelsetRelationsGet(&relations,&lines);
            if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
            if(!error&&lines)error=ProArraySizeGet((ProArray)lines,&count);
            if(lines)ProWstringproarrayFree(lines);
            if(!error&&count)error=PRO_TK_UNSUPPORTED;
        }else if(lookup!=PRO_TK_E_NOT_FOUND&&lookup!=PRO_TK_NOT_IMPLEMENTED&&lookup!=PRO_TK_INVALID_TYPE)error=lookup;
    }
    if(!error)error=sh_begin(&tx,replay->target,label[0]?label:L"BWTL_SWEEP",type);
    if(!error){stage="rebuild sweep path and orientation";error=replay_sweep_tree(replay,original,&source,&tx);}
    if(!error){stage="create incomplete native sweep";error=fd_create(&tx,1);}
    if(!error)error=ProFeatureElemtreeExtract(&tx.feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&extracted);
    if(!error)error=replay_sweep_section(extracted,&sketch,&section);
    if(!error){stage="verify sweep section frame";error=replay_section_frame(source.section,section);}
    if(!error){stage="rebuild editable sweep section";error=replay_draw_section(replay,source.section,section,
        REPLAY_SWEEP_SECTION,entities,dimensions);}
    if(!error)error=ProElementSpecialvalueSet(sketch,section);
    if(!error){
        stage="finish native sweep";
        tx.options[0]=PRO_FEAT_CR_NO_OPTS;
        error=ProFeatureWithoptionsRedefine(NULL,&tx.feature,extracted,tx.options,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&tx.errors);
    }
    if(!error)error=part_regenerate(tx.model);
    if(!error){
        ProFeattype actual;
        error=ProFeatureTypeGet(&tx.feature,&actual);
        if(!error&&actual!=type)error=PRO_TK_INCOMPLETE;
    }
    if(!error)*created=tx.feature;
    if(error)trace(stage,error);
    if(extracted)ProFeatureElemtreeFree(&tx.feature,extracted);
    fd_cleanup(&tx,error);
    return error;
}
