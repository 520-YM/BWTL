static ProError replay_hole_profile_element(ProElement tree,ProElement *section)
{
    const ProElemId path[]={PRO_E_HLE_COM,PRO_E_SKETCHER};
    return mc_tree_element(tree,path,_countof(path),section);
}

static int replay_is_hole_profile(ProElement tree)
{
    ProElement kind=NULL;
    int type;
    return !mc_tree_element(tree,(ProElemId[]){PRO_E_HLE_COM,PRO_E_HLE_TYPE_NEW},2,&kind)&&
        !ProElementIntegerGet(kind,NULL,&type)&&type==PRO_HLE_NEW_TYPE_SKETCH;
}

static ProError replay_hole_profile_placement(ProElement tree,ProElement *placement)
{
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_HLE_PLACEMENT},1,placement);
    ProElement kind;
    int type;
    if(!error)error=mc_tree_element(*placement,(ProElemId[]){PRO_E_HLE_PL_TYPE},1,&kind);
    if(!error)error=ProElementIntegerGet(kind,NULL,&type);
    if(!error&&type!=PRO_HLE_PL_TYPE_LIN)error=PRO_TK_UNSUPPORTED;
    return error;
}

static ProError replay_hole_profile_attach(ProElement tree,ProSection profile,ProElement *section)
{
    ProElement hole;
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_HLE_COM},1,&hole);
    if(!error)error=model_element(hole,PRO_E_SKETCHER,0,0,section);
    if(!error)error=ProElementSpecialvalueSet(*section,profile);
    return error;
}


static ProError replay_hole_profile_tree(ImitationReplay *replay,ProElement source,ProElement *target)
{
    static const ProElemId root_fields[]={PRO_E_FEATURE_TYPE,PRO_E_FEATURE_FORM,PRO_E_STD_FEATURE_NAME};
    static const ProElemId profile_fields[]={PRO_E_HLE_TYPE_NEW,PRO_E_HLE_MAKE_LIGHTWT,
        PRO_E_HLE_CRDIR_FLIP,PRO_E_HLE_TOP_CLEARANCE};
    static const ProElemId placement_fields[]={PRO_E_HLE_PRIM_REF,PRO_E_HLE_PL_TYPE,
        PRO_E_HLE_DIM_REF1,PRO_E_HLE_PLC_ALIGN_OPT1,PRO_E_HLE_DIM_DIST1,
        PRO_E_HLE_DIM_REF2,PRO_E_HLE_PLC_ALIGN_OPT2,PRO_E_HLE_DIM_DIST2};
    ProElement original_hole,original_placement,hole,placement;
    ProError error=mc_tree_element(source,(ProElemId[]){PRO_E_HLE_COM},1,&original_hole);
    if(!error)error=replay_hole_profile_placement(source,&original_placement);
    if(!error)error=ProElementAlloc(PRO_E_FEATURE_TREE,target);
    for(int i=0;!error&&i<_countof(root_fields);i++)
        error=replay_pattern_field(replay,source,*target,root_fields[i]);
    if(!error)error=model_element(*target,PRO_E_HLE_COM,0,0,&hole);
    for(int i=0;!error&&i<_countof(profile_fields);i++)
        error=replay_pattern_field(replay,original_hole,hole,profile_fields[i]);
    if(!error)error=model_element(*target,PRO_E_HLE_PLACEMENT,0,0,&placement);
    for(int i=0;!error&&i<_countof(placement_fields);i++)
        error=replay_pattern_field(replay,original_placement,placement,placement_fields[i]);
    if(!error)error=replay_pattern_field(replay,source,*target,PRO_E_BODY);
    return error;
}

