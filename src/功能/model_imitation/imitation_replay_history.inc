static ProError replay_history_open(ImitationReplay *replay,ProFeature *feature,FdSketch *history){
    static volatile LONG serial;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"bwtl_hist_%08lx_%08lx",GetCurrentProcessId(),
        (unsigned long)InterlockedIncrement(&serial));
    ProError error=ProMdlnameCopy(replay->source,name,&replay->history_owner);
    if(!error)error=ProFeatureInit((ProSolid)replay->history_owner,feature->id,&history->feature);
    if(!error)error=ProFeatureElemtreeExtract(&history->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&history->tree);
    ProFeature previous;
    int last=replay->rebuilding_remove&&replay->last_pattern_member?replay->last_pattern_member:replay->last_feature;
    if(!error&&!last)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProFeatureInit((ProSolid)replay->history_owner,last,&previous);
    if(!error)error=ProFeatureInsertmodeWithoptionsActivate(&previous,PRO_REGEN_NO_RESOLVE_MODE);
    return error;
}

static ProError replay_history_chain(ImitationReplay *replay,ProCollection original,ProCollection target){
    ProSelection *items=NULL;
    int count=0;
    ProError error=ProCrvcollectionRegenerate(original,&items,&count);
    if(!error&&!count)error=PRO_TK_EMPTY;
    for(int i=0;!error&&i<count;i++){
        ProModelitem item;
        ProReference old=NULL,fresh=NULL;
        ProCrvcollinstr instruction=NULL;
        error=ProSelectionModelitemGet(items[i],&item);
        if(!error&&(item.owner!=replay->history_owner||item.type!=PRO_EDGE))error=PRO_TK_UNSUPPORTED;
        if(!error)error=ProSelectionToReference(items[i],&old);
        if(!error)error=replay_map_reference(replay,old,&fresh);
        if(!error)error=ProCrvcollinstrAlloc(PRO_CURVCOLL_ADD_ONE_INSTR,&instruction);
        if(!error)error=ProCrvcollinstrReferenceAdd(instruction,fresh);
        if(!error)error=ProCrvcollectionInstructionAdd(target,instruction);
        if(old)ProReferenceFree(old);
        if(fresh)ProReferenceFree(fresh);
        if(instruction)ProCrvcollinstrFree(instruction);
    }
    if(items)ProSelectionarrayFree(items);
    if(!error)part_text_add(replay->out,L"源特征 %d 从其创建前的状态重建 %d 条边参照。\n",
        replay->current_feature,count);
    return error;
}
