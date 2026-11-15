static int replay_quilt_trim(ProElement tree)
{
    ProElement selected=NULL;
    ProReference reference=NULL;
    int id=0;
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_TRIM_QUILT},1,&selected);
    if(!error)error=ProElementReferenceGet(selected,NULL,&reference);
    if(!error&&reference)error=ProReferenceIdGet(reference,&id);
    if(reference)ProReferenceFree(reference);
    return !error&&id>0;
}

static int replay_quilt_extend(ProElement tree)
{
    int mode=0,state=0,measure=0;
    double distance=0;
    ProElement value=NULL,chain=NULL;
    return !replay_tree_integer(tree,PRO_E_SURF_EXTEND_TYPE,&mode)&&mode==2&&
        !replay_tree_integer(tree,PRO_E_SURF_EXTEND_DIST_STATE,&state)&&state==1&&
        !replay_tree_integer(tree,PRO_E_SURF_EXTEND_WAY_OF_MEASURING,&measure)&&measure==1&&
        !mc_tree_element(tree,(ProElemId[]){PRO_E_SURF_EXTEND_SINGLE_DIST_VALUE},1,&value)&&
        !ProElementDoubleGet(value,NULL,&distance)&&isfinite(distance)&&distance>0&&
        !mc_tree_element(tree,(ProElemId[]){PRO_E_STD_CURVE_COLLECTION_APPL},1,&chain);
}

static ProError replay_quilt_extend_tree(ImitationReplay *replay,ProElement source,double distance,ProElement *target)
{
    const ProElemId fields[]={PRO_E_FEATURE_TYPE,PRO_E_STD_FEATURE_NAME,PRO_E_STD_CURVE_COLLECTION_APPL,
        PRO_E_SURF_EXTEND_TYPE,PRO_E_SURF_EXTEND_DIST_STATE,PRO_E_SURF_EXTEND_WAY_OF_MEASURING,
        PRO_E_SURF_EXTEND_FLIP,PRO_E_SURF_EXTEND_SINGLE_DIST_VALUE};
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,target);
    for(int i=0;!error&&i<_countof(fields);i++){
        ProElement original=NULL,fresh=NULL;
        if(fields[i]==PRO_E_SURF_EXTEND_SINGLE_DIST_VALUE){
            error=model_element(*target,fields[i],2,distance,NULL);
            continue;
        }
        error=mc_tree_element(source,&fields[i],1,&original);
        if(error==PRO_TK_E_NOT_FOUND&&fields[i]==PRO_E_STD_FEATURE_NAME){
            error=PRO_TK_NO_ERROR;
            continue;
        }
        if(!error)error=replay_element(replay,original,&fresh);
        if(!error&&fresh){
            error=ProElemtreeElementAdd(*target,NULL,fresh);
            if(error)ProElementFree(&fresh);
        }
    }

    return error;
}

static ProError replay_extend_distance(ProFeature *source,double *distance){
    ProElement tree=NULL,element=NULL;
    ProError error=ProFeatureElemtreeExtract(source,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    if(!error&&!replay_quilt_extend(tree))error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(tree,(ProElemId[]){PRO_E_SURF_EXTEND_SINGLE_DIST_VALUE},1,&element);
    if(!error)error=ProElementDoubleGet(element,NULL,distance);

    if(tree)ProFeatureElemtreeFree(source,tree);
    return error;
}

static ProError replay_capture_extend(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProFeattype type;
    ProError error=ProFeatureTypeGet(feature,&type);
    if(error||type!=PRO_FEAT_EXTEND)return error;
    ImitationReplay *replay=data;
    if(replay->extend_count==_countof(replay->extensions))return PRO_TK_OUT_OF_RANGE;
    double distance=0;
    error=replay_extend_distance(feature,&distance);
    replay->extensions[replay->extend_count++]=(ReplayExtendInput){feature->id,distance,error};
    return PRO_TK_NO_ERROR;
}

static ProError replay_original_extend(const ImitationReplay *replay,int id,double *distance){
    for(int i=0;i<replay->extend_count;i++)if(replay->extensions[i].id==id){
        *distance=replay->extensions[i].distance;
        return replay->extensions[i].error;
    }
    return PRO_TK_E_NOT_FOUND;
}