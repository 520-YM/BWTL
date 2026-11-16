static ProError replay_removed_surface_reference(ImitationReplay *replay,ProReference old,ProReference *mapped){
    ProMdl owner=NULL;
    ProError error=ProReferenceOwnerGet(old,&owner);
    if(error){
        ProFamilyMdlName declared=L"";
        ProMdlName source=L"",history=L"";
        error=ProReferenceOriginalownerMdlnameGet(old,declared);
        if(!error)error=ProMdlMdlnameGet(replay->source,source);
        if(!error)error=ProMdlMdlnameGet(replay->history_owner,history);
        if(!error&&_wcsicmp(declared,source)&&_wcsicmp(declared,history))error=PRO_TK_UNSUPPORTED;
    }else if(owner!=replay->source&&owner!=replay->history_owner)error=PRO_TK_UNSUPPORTED;
    int id=0;
    ProType type;
    if(!error&&ProReferenceOriginalidGet(old,&id))error=ProReferenceIdGet(old,&id);
    if(!error)error=ProReferenceTypeGet(old,&type);
    if(!error&&(type!=PRO_SURFACE||id<=0))error=PRO_TK_UNSUPPORTED;
    ProModelitem item;
    ProReference historical=NULL;
    if(!error)error=ProModelitemInit(replay->history_owner,id,PRO_SURFACE,&item);
    if(!error)error=ProReferenceAlloc(&historical);
    if(!error)error=ProReferenceSet(historical,NULL,&item);
    if(!error)error=replay_map_reference(replay,historical,mapped);
    if(historical)ProReferenceFree(historical);
    return error;
}
static ProError replay_surface_instruction(ImitationReplay *replay,ProSrfcollinstr original,
                                           ProCollection collection)
{
    ProSrfcollinstrType kind;
    ProBoolean include=PRO_B_TRUE;
    ProSrfcollref *inputs=NULL;
    ProSrfcollinstr created=NULL;
    int count=0;
    ProError error=ProSrfcollinstrTypeGet(original,&kind);
    if(!error&&kind!=PRO_SURFCOLL_SINGLE_SURF)error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSrfcollinstrIncludeGet(original,&include);
    if(!error)error=ProSrfcollinstrReferencesGet(original,&inputs);
    if(!error)error=ProArraySizeGet((ProArray)inputs,&count);
    if(!error&&(count<1||count>REPLAY_FEATURE_MAX))error=PRO_TK_UNSUPPORTED;
    if(!error)error=ProSrfcollinstrAlloc(kind,include,&created);
    for(int i=0;!error&&i<count;i++){
        ProSrfcollrefType ref_kind;
        ProSrfcollref selected=NULL;
        ProReference old=NULL,mapped=NULL;
        error=ProSrfcollrefTypeGet(inputs[i],&ref_kind);
        if(!error&&ref_kind!=PRO_SURFCOLL_REF_SINGLE)error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProSrfcollrefItemGet(inputs[i],&old);
        if(!error&&replay->history_owner&&replay->rebuilding_remove)
            error=replay_removed_surface_reference(replay,old,&mapped);
        else if(!error)error=replay_map_reference(replay,old,&mapped);
        if(!error)error=ProSrfcollrefAlloc(ref_kind,mapped,&selected);
        if(!error){
            error=ProSrfcollinstrReferenceAdd(created,selected);
            if(!error)selected=NULL;
        }
        if(selected)ProSrfcollrefFree(selected);
        if(mapped)ProReferenceFree(mapped);
        if(old)ProReferenceFree(old);
    }
    if(!error){
        error=ProSrfcollectionInstructionAdd(collection,created);
        if(!error)created=NULL;
    }
    if(created)ProSrfcollinstrFree(created);
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
