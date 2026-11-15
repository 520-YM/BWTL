static int replay_boolean_definition(ProElement tree,int *operation)
{
    ProElement target,tool;
    int keep=PRO_KEEP_TOOL_NO;
    if(replay_tree_integer(tree,PRO_E_BOOLEAN_TYPE,operation)||
       (*operation!=PRO_MERGE_BOOL_TYPE&&*operation!=PRO_SUBTRACT_BOOL_TYPE&&*operation!=PRO_INTERSECT_BOOL_TYPE)||
       mc_tree_element(tree,(ProElemId[]){PRO_E_TARGET_BODY},1,&target)||
       mc_tree_element(tree,(ProElemId[]){PRO_E_TOOL_BODIES},1,&tool))return 0;
    ProElement option;
    if(!mc_tree_element(tree,(ProElemId[]){PRO_E_KEEP_TOOLS},1,&option)&&
       ProElementIntegerGet(option,NULL,&keep))return 0;
    return keep==PRO_KEEP_TOOL_NO;
}

static ProError replay_boolean_origin(ProSolidBody *body,int *feature_id)
{
    ProFeature *features=NULL;
    int count=0,first=INT_MAX;
    *feature_id=0;
    ProError error=ProSolidBodyFeaturesGet(body,&features);
    if(!error&&features)error=ProArraySizeGet((ProArray)features,&count);
    for(int i=0;!error&&i<count;i++){
        int number=0;
        ProFeattype type;
        error=ProFeatureTypeGet(&features[i],&type);
        if(error||(type!=PRO_FEAT_FIRST_FEAT&&type!=PRO_FEAT_PROTRUSION))continue;
        error=ProFeatureNumberGet(&features[i],&number);
        if(!error&&number<first){first=number;*feature_id=features[i].id;}
    }
    if(features)ProArrayFree((ProArray *)&features);
    return error?error:*feature_id?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static ProError replay_body_by_origin(ImitationReplay *replay,ProElement tree,ProElemId field,FdRef *target)
{
    ProElement element;
    ProReference ref=NULL;
    ProMdl owner=NULL;
    ProSolidBody body,*bodies=NULL;
    ProType type;
    int id=0,original=0,mapped=0,count=0,matches=0;
    ProError error=mc_tree_element(tree,&field,1,&element);
    if(!error)error=ProElementReferenceGet(element,NULL,&ref);
    if(!error)error=ProReferenceOwnerGet(ref,&owner);
    if(!error)error=ProReferenceTypeGet(ref,&type);
    if(!error&&(owner!=replay->source||type!=PRO_BODY))error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProReferenceIdGet(ref,&id);
    if(!error)error=ProModelitemInit(owner,id,PRO_BODY,&body);
    if(!error)error=replay_boolean_origin(&body,&original);
    for(int i=0;!error&&i<replay->count;i++)
        if(replay->mapped[i].source==original)mapped=replay->mapped[i].target;
    if(!error&&!mapped)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProSolidBodiesCollect((ProSolid)replay->target,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        int origin=0;
        ProError read=replay_boolean_origin(&bodies[i],&origin);
        if(read&&read!=PRO_TK_E_NOT_FOUND){error=read;break;}
        if(origin==mapped){
            target->item=bodies[i];
            target->owner_feature=origin;
            matches++;
        }
    }
    if(!error&&matches!=1)error=PRO_TK_E_AMBIGUOUS;
    if(!error)error=ProSelectionAlloc(NULL,&target->item,&target->selection);
    if(bodies)ProArrayFree((ProArray *)&bodies);
    if(ref)ProReferenceFree(ref);
    return error;
}

static ProError replay_boolean(ImitationReplay *replay,ProElement original,const wchar_t *name,ProFeature *created)
{
    FdTxn tx={0};
    FdRef target={0},tool={0};
    int operation,active=0;
    ProError error=sd_body_count(replay->target,&active);
    trace("Replay boolean input body count",active);
    if(!error&&(active!=2||!replay_boolean_definition(original,&operation)))error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_body_by_origin(replay,original,PRO_E_TARGET_BODY,&target);
    trace("Replay boolean target body",error);
    if(!error)error=replay_body_by_origin(replay,original,PRO_E_TOOL_BODIES,&tool);
    trace("Replay boolean tool body",error);
    if(!error&&target.item.id==tool.item.id)error=PRO_TK_E_AMBIGUOUS;
    if(!error)error=sh_begin(&tx,replay->target,name,PRO_FEAT_BOOLEANBODIES);
    if(!error)error=model_element(tx.root,PRO_E_BOOLEAN_TYPE,1,operation,NULL);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_TARGET_BODY,&target);
    if(!error)error=fd_ref_element(&tx,tx.root,PRO_E_TOOL_BODIES,&tool);
    if(!error&&operation==PRO_SUBTRACT_BOOL_TYPE)
        error=model_element(tx.root,PRO_E_KEEP_TOOLS,1,PRO_KEEP_TOOL_NO,NULL);
    if(!error)error=fd_create(&tx,0);
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=sd_body_count(replay->target,&active);
    if(!error&&active!=1)error=PRO_TK_INCOMPLETE;
    if(!error)error=sd_projection_reference(replay->target,&tx.feature,&target);
    if(!error)error=sd_projection_reference(replay->target,&tx.feature,&tool);
    if(!error)*created=tx.feature;
    fd_ref_free(&target);
    fd_ref_free(&tool);
    /* On error the caller discards the independently built part. */
    fd_cleanup(&tx,PRO_TK_NO_ERROR);
    return error;
}
