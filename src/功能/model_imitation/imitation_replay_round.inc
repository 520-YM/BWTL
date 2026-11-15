static int replay_variable_round(ProElement tree)
{
    ProElement sets=NULL,*members=NULL,shape=NULL;
    int count=0,kind=0,variable=0;
    if(!mc_tree_element(tree,(ProElemId[]){PRO_E_RNDCH_SETS},1,&sets)&&
       !ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&members)&&
       !ProElementArrayGet(sets,NULL,&members)&&members&&
       !ProArraySizeGet((ProArray)members,&count)){
        for(int i=0;i<count;i++)
            if(!mc_tree_element(members[i],(ProElemId[]){PRO_E_RNDCH_SHAPE_OPTIONS},1,&shape)&&
               !ProElementIntegerGet(shape,NULL,&kind)&&kind==PRO_ROUND_TYPE_VARIABLE)variable=1;
    }
    if(members)ProArrayFree((ProArray *)&members);
    return variable;
}

static ProError replay_variable_round_tree(ImitationReplay *replay,ProElement source,const wchar_t *name,FdTxn *tx)
{
    ProElement sets=NULL,*members=NULL,*radii=NULL,shape=NULL,collection=NULL,radius_group=NULL;
    ProCollection chain=NULL;
    ProCrvcollinstr *instructions=NULL;
    ProReference *references=NULL;
    FdRef mapped={0};
    int count=0,edge_count=0,kind=0;
    double values[2]={0};
    unsigned endpoints=0;
    ProError error=mc_tree_element(source,(ProElemId[]){PRO_E_RNDCH_SETS},1,&sets);
    if(!error)error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&members);
    if(!error)error=ProElementArrayGet(sets,NULL,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=mc_tree_element(members[0],(ProElemId[]){PRO_E_RNDCH_SHAPE_OPTIONS},1,&shape);
    if(!error)error=ProElementIntegerGet(shape,NULL,&kind);
    if(!error&&kind!=PRO_ROUND_TYPE_VARIABLE)error=PRO_TK_UNSUPPORTED;
    ProElement option=NULL;
    if(!error&&!mc_tree_element(members[0],(ProElemId[]){PRO_E_RNDCH_COMPOUND_CONIC,PRO_E_RNDCH_CONIC_TYPE},2,&option)){
        error=ProElementIntegerGet(option,NULL,&kind);
        if(!error&&kind!=PRO_ROUND_CONIC_DISABLE)error=PRO_TK_UNSUPPORTED;
    }
    if(!error)error=mc_tree_element(members[0],(ProElemId[]){PRO_E_RNDCH_REFERENCES,PRO_E_STD_CURVE_COLLECTION_APPL},2,&collection);
    if(!error)error=ProElementCollectionGet(collection,NULL,&chain);
    if(!error)error=ProCrvcollectionInstructionsGet(chain,&instructions);
    if(!error)error=ProArraySizeGet((ProArray)instructions,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProCrvcollinstrReferencesGet(instructions[0],&references);
    if(!error)error=ProArraySizeGet((ProArray)references,&edge_count);
    if(!error&&edge_count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=replay_reference_item(replay,references[0],&mapped.item,0);
    if(!error)error=ProSelectionAlloc(NULL,&mapped.item,&mapped.selection);
    if(!error)error=mc_tree_element(members[0],(ProElemId[]){PRO_E_RNDCH_RADII},1,&radius_group);
    if(!error)error=ProArrayAlloc(0,sizeof(ProElement),1,(ProArray *)&radii);
    if(!error)error=ProElementArrayGet(radius_group,NULL,&radii);
    if(!error)error=ProArraySizeGet((ProArray)radii,&count);
    if(!error&&count!=2)error=PRO_TK_UNSUPPORTED;
    ProEdge edge=NULL;
    ProVector ends[2];
    ProMdl owner=NULL;
    int edge_id=0;
    ProEdge source_edge=NULL;
    if(!error)error=ProReferenceOwnerGet(references[0],&owner);
    if(!error&&!owner)error=PRO_TK_BAD_CONTEXT;
    if(!error)error=ProReferenceIdGet(references[0],&edge_id);
    if(!error)error=ProEdgeInit((ProSolid)owner,edge_id,&source_edge);
    if(!error)error=ProGeomitemToEdge(&mapped.item,&edge);
    for(int i=0;!error&&i<2;i++)error=ProEdgeXyzdataEval(edge,i,ends[i],NULL,NULL,NULL);
    for(int i=0;!error&&i<2;i++){
        ProElement point=NULL,value=NULL;
        ProReference location=NULL;
        ProVector xyz;
        error=mc_tree_element(radii[i],(ProElemId[]){PRO_E_STD_POINT_COLLECTION_APPL},1,&point);
        if(!error)error=ProElementReferenceGet(point,NULL,&location);
        ProUvParam parameter;
        if(!error)error=ProReferenceParamsGet(location,parameter);
        if(!error&&(!isfinite(parameter[0])||(fabs(parameter[0])>1e-7&&fabs(parameter[0]-1)>1e-7)))
            error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProEdgeXyzdataEval(source_edge,parameter[0]>0.5,xyz,NULL,NULL,NULL);
        if(!error){
            int start=ad_near(xyz,ends[0],0.0002),end=ad_near(xyz,ends[1],0.0002);
            if(start==end||(endpoints&(1u<<end)))error=PRO_TK_UNSUPPORTED;
            else{
                endpoints|=1u<<end;
                error=mc_tree_element(radii[i],(ProElemId[]){PRO_E_RNDCH_LEG1,PRO_E_RNDCH_LEG_VALUE},2,&value);
                if(!error)error=ProElementDoubleGet(value,NULL,&values[end]);
            }
        }
        if(location)ProReferenceFree(location);
    }
    if(!error)error=ProElementAlloc(PRO_E_FEATURE_TREE,&tx->root);
    if(!error)error=model_element(tx->root,PRO_E_FEATURE_TYPE,1,PRO_FEAT_ROUND,NULL);
    if(!error&&!replay_tree_integer(source,PRO_E_RNDCH_ATTACH_TYPE,&kind)&&
       (kind==PRO_ROUND_UNATTACHED||kind==PRO_ROUND_CAPPED_ENDS))error=PRO_TK_UNSUPPORTED;
    ProElement label=NULL;
    if(!error&&name[0]){
        error=model_element(tx->root,PRO_E_STD_FEATURE_NAME,0,0,&label);
        if(!error)error=ProElementWstringSet(label,(wchar_t *)name);
    }
    ProElement target_sets=NULL,set=NULL,refs=NULL,conic=NULL;
    if(!error)error=model_element(tx->root,PRO_E_RNDCH_SETS,0,0,&target_sets);
    if(!error)error=model_element(target_sets,PRO_E_RNDCH_SET,0,0,&set);
    if(!error)error=model_element(set,PRO_E_RNDCH_SHAPE_OPTIONS,1,PRO_ROUND_TYPE_VARIABLE,NULL);
    if(!error)error=model_element(set,PRO_E_RNDCH_COMPOUND_CONIC,0,0,&conic);
    if(!error)error=model_element(conic,PRO_E_RNDCH_CONIC_TYPE,1,PRO_ROUND_CONIC_DISABLE,NULL);
    if(!error)error=model_element(set,PRO_E_RNDCH_REFERENCES,0,0,&refs);
    if(!error)error=model_element(refs,PRO_E_RNDCH_REFERENCE_TYPE,1,PRO_ROUND_REF_EDGE,NULL);
    if(!error)error=model_element(refs,PRO_E_STD_CURVE_COLLECTION_APPL,0,0,&collection);
    if(!error)error=am_edge_chain(collection,&mapped,0);
    if(!error)error=model_element(set,PRO_E_RNDCH_RADII,0,0,&radius_group);
    for(int i=0;!error&&i<2;i++)error=am_radius(tx,radius_group,values[i],0,0,i,&mapped);
    if(!error)error=model_element(tx->root,PRO_E_RNDCH_ATTACH_TYPE,1,PRO_ROUND_ATTACHED,NULL);
    if(references)ProReferencearrayFree(references);
    if(instructions)ProCrvcollinstrArrayFree(instructions);
    if(chain)ProCollectionFree(&chain);
    if(radii)ProArrayFree((ProArray *)&radii);
    if(members)ProArrayFree((ProArray *)&members);
    fd_ref_free(&mapped);
    return error;
}
