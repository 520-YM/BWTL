static ProError replay_mirror_seed(ProElement tree,ProReference **seed,ProType *kind)
{
    ProElement items=NULL;
    ProReference *refs=NULL;
    int count=0;
    ProError error=mc_tree_element(tree,(ProElemId[]){PRO_E_MIRROR_REF_ITEMS},1,&items);
    if(!error)error=ProElementReferencesGet(items,NULL,&refs);
    if(!error&&refs)error=ProArraySizeGet((ProArray)refs,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProReferenceTypeGet(refs[0],kind);
    if(!error&&*kind!=PRO_BODY&&*kind!=PRO_QUILT)error=PRO_TK_UNSUPPORTED;
    int attach=0;
    if(!error&&*kind==PRO_BODY&&!replay_tree_integer(tree,PRO_E_MIRROR_BODY_ATTACH_OPT,&attach)&&attach)
        error=PRO_TK_UNSUPPORTED;
    if(!error&&seed)*seed=refs;
    else if(refs)ProReferencearrayFree(refs);
    return error;
}

static ProError replay_geometry_mirror_tree(ImitationReplay *replay,ProElement original,FdTxn *tx)
{
    ProReference *source=NULL,reference=NULL;
    ProElement item=NULL;
    ProType kind;
    FdRef target_body={0};
    ProError error=replay_mirror_seed(original,&source,&kind);
    if(!error&&kind==PRO_BODY){
        int id=0;
        ProMdl owner=NULL;
        ProMdlsubtype subtype;
        ProSolidBody original_body;
        ProMaterial body_material={0},part_material={0};
        error=ProReferenceIdGet(source[0],&id);
        if(!error)error=ProReferenceOwnerGet(source[0],&owner);
        if(!error&&owner!=replay->source)error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProMdlSubtypeGet(replay->source,&subtype);
        if(!error&&subtype!=PROMDLSTYPE_PART_SOLID)error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProSolidDefaultBodyGet((ProSolid)replay->source,&original_body);
        if(!error&&id!=original_body.id)error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProMaterialCurrentGet((ProSolid)replay->source,&part_material);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        if(!error)error=ProSolidBodyMaterialGet(&original_body,&body_material);
        if(!error&&((body_material.matl_name[0]&&_wcsicmp(body_material.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))||
           (part_material.matl_name[0]&&_wcsicmp(part_material.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))))
            error=PRO_TK_UNSUPPORTED;
        if(!error)error=qa_single_body(replay->target,&target_body.item);
        if(!error)error=ProSelectionAlloc(NULL,&target_body.item,&target_body.selection);
        if(!error)error=ProSelectionToReference(target_body.selection,&reference);
    }
    if(!error&&kind==PRO_QUILT)error=replay_map_reference(replay,source[0],&reference);
    if(!error)error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    const ProElemId fields[]={PRO_E_FEATURE_TYPE,PRO_E_SRF_TRANS_TYPE,PRO_E_STD_FEATURE_NAME,
        PRO_E_MIRROR_REF_PLANE,PRO_E_COPY_NO_COPY};
    for(int i=0;!error&&i<_countof(fields);i++){
        ProElement value=NULL,local=NULL;
        error=mc_tree_element(original,&fields[i],1,&value);
        if(error==PRO_TK_E_NOT_FOUND&&fields[i]==PRO_E_STD_FEATURE_NAME){
            error=PRO_TK_NO_ERROR;
            continue;
        }
        if(!error)error=replay_element(replay,value,&local);
        if(!error&&local)error=ProElemtreeElementAdd(tx->root,NULL,local);
        if(error&&local)ProElementFree(&local);
    }
    if(!error)error=model_element(tx->root,PRO_E_MIRROR_REF_ITEMS,0,0,&item);
    if(!error)error=ProElementReferenceSet(item,reference);
    if(source)ProReferencearrayFree(source);
    if(reference)ProReferenceFree(reference);
    fd_ref_free(&target_body);
    return error;
}
