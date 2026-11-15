static ProError replay_cut_tree(ImitationReplay *replay,ProElement source,FdTxn *tx)
{
    const ProElemId required[]={
        PRO_E_FEATURE_TYPE,PRO_E_FEATURE_FORM,PRO_E_STD_FEATURE_NAME,PRO_E_EXT_SURF_CUT_SOLID_TYPE,
        PRO_E_FEAT_FORM_IS_THIN,PRO_E_REMOVE_MATERIAL,PRO_E_IS_SMT_CUT,PRO_E_SMT_CUT_NORMAL_DIR,
        PRO_E_STD_DIRECTION,PRO_E_STD_SECTION
    };
    ProElement body,depth,from,to,original_to;
    int from_type,to_type;
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    for(int i=0;!error&&i<_countof(required);i++){
        ProElement old,fresh=NULL;
        error=mc_tree_element(source,&required[i],1,&old);
        if(!error)error=replay_element(replay,old,&fresh);
        if(!error&&fresh)error=ProElemtreeElementAdd(tx->root,NULL,fresh);
        if(error&&fresh)ProElementFree(&fresh);
    }
    if(!error)error=mc_tree_element(source,(ProElemId[]){PRO_E_STD_EXT_DEPTH},1,&depth);
    if(!error)error=mc_tree_element(depth,(ProElemId[]){PRO_E_EXT_DEPTH_FROM,PRO_E_EXT_DEPTH_FROM_TYPE},2,&from);
    if(!error)error=mc_tree_element(depth,(ProElemId[]){PRO_E_EXT_DEPTH_TO},1,&original_to);
    if(!error)error=mc_tree_element(original_to,(ProElemId[]){PRO_E_EXT_DEPTH_TO_TYPE},1,&to);
    if(!error)error=ProElementIntegerGet(from,NULL,&from_type);
    if(!error)error=ProElementIntegerGet(to,NULL,&to_type);
    if(error)return error;
    if(from_type!=PRO_EXT_DEPTH_FROM_NONE||(to_type!=PRO_EXT_DEPTH_TO_NEXT&&to_type!=PRO_EXT_DEPTH_TO_ALL&&
       to_type!=PRO_EXT_DEPTH_TO_BLIND))return PRO_TK_UNSUPPORTED;
    if((error=model_element(tx->root,PRO_E_BODY,0,0,&body))||
       (error=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL))||
       (error=sd_existing_body(tx,body)))return error;
    if((error=model_element(tx->root,PRO_E_STD_EXT_DEPTH,0,0,&depth))||
       (error=model_element(depth,PRO_E_EXT_DEPTH_FROM,0,0,&from))||
       (error=model_element(from,PRO_E_EXT_DEPTH_FROM_TYPE,1,from_type,NULL))||
       (error=model_element(depth,PRO_E_EXT_DEPTH_TO,0,0,&to))||
       (error=model_element(to,PRO_E_EXT_DEPTH_TO_TYPE,1,to_type,NULL)))return error;
    if(to_type!=PRO_EXT_DEPTH_TO_BLIND)return PRO_TK_NO_ERROR;
    ProElement original=NULL,fresh=NULL;
    error=mc_tree_element(original_to,(ProElemId[]){PRO_E_EXT_DEPTH_TO_VALUE},1,&original);
    if(!error)error=replay_element(replay,original,&fresh);
    if(!error&&!fresh)error=PRO_TK_INCOMPLETE;
    if(!error)error=ProElemtreeElementAdd(to,NULL,fresh);
    if(error&&fresh)ProElementFree(&fresh);
    return error;
}
