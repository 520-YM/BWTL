static ProError replay_extrusion_new_body(ProFeature *feature,ProElement tree,int *separate)
{
    int form=0,material=0,mode=0,number=0;
    ProElement body=NULL,selection=NULL;
    ProReference reference=NULL;
    ProError error=replay_tree_integer(tree,PRO_E_FEATURE_FORM,&form);
    if(error||form!=PRO_EXTRUDE)return PRO_TK_NO_ERROR;
    error=replay_tree_integer(tree,PRO_E_EXT_SURF_CUT_SOLID_TYPE,&material);
    if(error||material!=PRO_EXT_FEAT_TYPE_SOLID)return PRO_TK_NO_ERROR;
    if(!replay_tree_integer(tree,PRO_E_REMOVE_MATERIAL,&material)&&material==PRO_EXT_MATERIAL_REMOVE)
        return PRO_TK_NO_ERROR;
    if(mc_tree_element(tree,(ProElemId[]){PRO_E_BODY},1,&body)||replay_tree_integer(body,PRO_E_BODY_USE,&mode))
        return PRO_TK_NO_ERROR;
    if(mode==PRO_BODY_USE_NEW){*separate=1;return PRO_TK_NO_ERROR;}
    if(mode!=PRO_BODY_USE_SELECTED)return PRO_TK_NO_ERROR;
    int active=0;
    error=sd_body_count(feature->owner,&active);
    if(error)return error;
    int id=0;
    ProSolidBody original;
    ProFeature *contributors=NULL;
    int count=0,preceding=0,found=0;
    error=mc_tree_element(body,(ProElemId[]){PRO_E_BODY_SELECT},1,&selection);
    if(!error)error=ProElementReferenceGet(selection,NULL,&reference);
    if(!error)error=ProReferenceIdGet(reference,&id);
    if(!error)error=ProModelitemInit(feature->owner,id,PRO_BODY,&original);
    if(!error)error=ProSolidBodyFeaturesGet(&original,&contributors);
    if(!error)error=ProArraySizeGet((ProArray)contributors,&count);
    if(!error)error=ProFeatureNumberGet(feature,&number);
    for(int i=0;!error&&i<count;i++){
        int candidate=0;
        error=ProFeatureNumberGet(&contributors[i],&candidate);
        if(!error&&candidate<number)preceding++;
        if(!error&&contributors[i].id==feature->id)found++;
    }
    /* The final body count excludes consumed boolean tools. Inspect the
       contributing features even when only the result body is active. */
    if(!error&&found!=1&&active>=2)error=PRO_TK_INCOMPLETE;
    if(!error&&found==1&&!preceding&&active>=2)*separate=1;
    if(!error&&found==1&&!preceding&&active<2){
        ProSolidBody first;
        error=ProSolidDefaultBodyGet((ProSolid)feature->owner,&first);
        if(!error)*separate=original.id!=first.id;
    }
    if(contributors)ProArrayFree((ProArray *)&contributors);
    if(reference)ProReferenceFree(reference);
    return error;
}

static ProError replay_extrusion_body_tree(ImitationReplay *replay,ProElement source,FdTxn *tx)
{
    ProElement *children=NULL,body=NULL;
    int count=0;
    ProError error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    if(!error)error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&children);
    if(!error)error=ProElementChildrenGet(source,NULL,&children);
    if(!error)error=ProArraySizeGet((ProArray)children,&count);
    for(int i=0;!error&&i<count;i++){
        ProElemId id=0;
        ProElement element=NULL;
        error=ProElementIdGet(children[i],&id);
        if(error||id==PRO_E_BODY)continue;
        error=replay_element(replay,children[i],&element);
        if(!error&&element)error=ProElemtreeElementAdd(tx->root,NULL,element);
        if(error&&element)ProElementFree(&element);
    }
    if(!error)error=model_element(tx->root,PRO_E_BODY,0,0,&body);
    if(!error)error=model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_NEW,NULL);
    if(children)ProArrayFree((ProArray *)&children);
    return error;
}

static ProError replay_extrusion_body_materials(ProMdl model)
{
    ProSolidBody *bodies=NULL;
    int count=0;
    ProMaterial part={0};
    ProError error=ProMaterialCurrentGet((ProSolid)model,&part);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&part.matl_name[0]&&_wcsicmp(part.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        ProMaterial material={0};
        error=ProSolidBodyMaterialGet(&bodies[i],&material);
        if(!error&&material.matl_name[0]&&_wcsicmp(material.matl_name,L"PTC_SYSTEM_MTRL_PROPS"))
            error=PRO_TK_UNSUPPORTED;
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error;
}
