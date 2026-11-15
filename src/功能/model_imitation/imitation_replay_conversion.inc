static int replay_empty_conversion(ProElement tree){
    int kind=0;
    return !replay_tree_integer(tree,PRO_E_DTM_SRF_TYPE,&kind)&&kind==PRO_DTM_SRF_EMPTY_BODY_CONV;
}

static int replay_wall_conversion(ProElement tree)
{
    int kind;
    ProElement surface;
    return !replay_tree_integer(tree,PRO_E_DTM_SRF_TYPE,&kind)&&kind==PRO_DTM_SRF_AS_WALL&&
        !mc_tree_element(tree,(ProElemId[]){PRO_E_SMT_DRV_SURF},1,&surface);
}

static ProError replay_convert_empty(ImitationReplay *replay,const wchar_t *label,ProFeature *created){
    ProSolidBody *bodies=NULL;
    ProSolidBodyState state;
    ProBoolean sheet=PRO_B_FALSE,construction=PRO_B_FALSE;
    int count=0;
    ProError error=template_empty((ProSolid)replay->target);
    if(!error)error=ProSolidBodiesCollect((ProSolid)replay->target,&bodies);
    if(!error&&bodies)error=ProArraySizeGet((ProArray)bodies,&count);
    if(!error&&count!=1)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProSolidBodyStateGet(&bodies[0],&state);
    if(!error)error=ProSolidBodyIsSheetmetal(&bodies[0],&sheet);
    if(!error)error=ProSolidBodyIsConstruction(&bodies[0],&construction);
    if(!error&&(sheet||construction||
        (state!=PRO_BODY_STATE_NO_CONTR_FEAT&&state!=PRO_BODY_STATE_NO_GEOMETRY)))error=PRO_TK_BAD_CONTEXT;
    FdTxn tx={0};
    ProElement body=NULL;
    ProReference reference=NULL;
    if(!error)error=sh_begin(&tx,replay->target,label[0]?label:L"BWTL_CONVERT_EMPTY",PRO_FEAT_DATUM_SURF);
    if(!error)error=model_element(tx.root,PRO_E_DTM_SRF_TYPE,1,PRO_DTM_SRF_EMPTY_BODY_CONV,NULL);
    if(!error)error=model_element(tx.root,PRO_E_SMT_CONV_BODY,0,0,&body);
    if(!error)error=ProReferenceAlloc(&reference);
    if(!error)error=ProReferenceSet(reference,NULL,&bodies[0]);
    if(!error)error=ProElementReferenceSet(body,reference);
    if(!error)error=sh_create(&tx);
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=ProSolidBodyIsSheetmetal(&bodies[0],&sheet);
    if(!error&&!sheet)error=PRO_TK_INCOMPLETE;
    if(!error)error=template_empty((ProSolid)replay->target);
    if(!error)*created=tx.feature;
    if(reference)ProReferenceFree(reference);
    if(bodies)ProArrayFree((ProArray *)&bodies);
    fd_cleanup(&tx,PRO_TK_NO_ERROR);
    return error;
}
