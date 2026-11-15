static ProError replay_surface_instruction(ImitationReplay *replay,ProSrfcollinstr original,
                                           ProCollection collection)
{
    ProSrfcollinstrType kind;
    ProBoolean include=PRO_B_TRUE;
    ProSrfcollrefType ref_kind;
    ProSrfcollref *inputs=NULL,selected=NULL;
    ProReference old=NULL,mapped=NULL;
    ProSrfcollinstr created=NULL;
    int count=0;
    ProError error=ProSrfcollinstrTypeGet(original,&kind);
    if(!error&&kind!=PRO_SURFCOLL_SINGLE_SURF)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSrfcollinstrIncludeGet(original,&include);
    if(!error)error=ProSrfcollinstrReferencesGet(original,&inputs);
    if(!error)error=ProArraySizeGet((ProArray)inputs,&count);
    if(!error&&count!=1)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSrfcollrefTypeGet(inputs[0],&ref_kind);
    if(!error&&ref_kind!=PRO_SURFCOLL_REF_SINGLE)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSrfcollrefItemGet(inputs[0],&old);
    if(!error)error=replay_map_reference(replay,old,&mapped);
    if(!error)error=ProSrfcollinstrAlloc(kind,include,&created);
    if(!error)error=ProSrfcollrefAlloc(ref_kind,mapped,&selected);
    if(!error){
        error=ProSrfcollinstrReferenceAdd(created,selected);
        if(!error)selected=NULL;
    }
    if(!error){
        error=ProSrfcollectionInstructionAdd(collection,created);
        if(!error)created=NULL;
    }
    if(selected)ProSrfcollrefFree(selected);
    if(created)ProSrfcollinstrFree(created);
    if(mapped)ProReferenceFree(mapped);
    if(old)ProReferenceFree(old);
    if(inputs)ProSrfcollrefArrayFree(inputs);
    return error;
}

static ProError replay_surface_collection(ImitationReplay *replay,ProElement source,ProElement target)
{
    ProCollection original=NULL,created=NULL;
    ProSrfcollinstr *instructions=NULL;
    int count=0;
    ProError error=ProElementCollectionGet(source,NULL,&original);
    if(!error)error=ProSrfcollectionInstructionsGet(original,&instructions);
    if(!error&&instructions)error=ProArraySizeGet((ProArray)instructions,&count);
    if(error==PRO_TK_EMPTY||error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(!error&&count)error=ProSrfcollectionAlloc(&created);
    for(int i=0;!error&&i<count;i++)error=replay_surface_instruction(replay,instructions[i],created);
    if(!error&&created)error=ProElementCollectionSet(target,created);
    if(instructions)ProSrfcollinstrArrayFree(instructions);
    if(original)ProCollectionFree(&original);
    if(created)ProCollectionFree(&created);
    return error;
}
