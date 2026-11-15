/* For a missing local reference, look at the original prefix in an isolated
   inspection model. Only the fresh feature still being constructed is retried. */
static ProError replay_reference_before_feature(ImitationReplay *replay,ProReference ref,ProModelitem *target,
    int edge_endpoint)
{
    ProMdl owner=NULL;
    ProType type;
    int id;
    if(ProReferenceOwnerGet(ref,&owner)||owner!=replay->source||
        ProReferenceTypeGet(ref,&type)||type!=PRO_EDGE||
        (ProReferenceOriginalidGet(ref,&id)&&ProReferenceIdGet(ref,&id))||id<=0)return PRO_TK_E_NOT_FOUND;
    static volatile LONG serial;
    ProMdlName name;
    swprintf_s(name,_countof(name),L"bwtl_ref_%08lx_%08lx",GetCurrentProcessId(),
        (unsigned long)InterlockedIncrement(&serial));
    ProMdl history=NULL;
    ProFeature previous;
    ProModelitem historic;
    ProReference historical_ref=NULL;
    ProError error=ProMdlnameCopy(replay->source,name,&history);
    if(!error)error=ProFeatureInit((ProSolid)history,replay->last_feature,&previous);
    if(!error)error=ProFeatureInsertmodeWithoptionsActivate(&previous,PRO_REGEN_NO_RESOLVE_MODE);
    if(!error)error=ProModelitemInit(history,id,type,&historic);
    if(!error)error=ProReferenceAlloc(&historical_ref);
    if(!error)error=ProReferenceSet(historical_ref,NULL,&historic);
    if(!error){
        replay->history_owner=history;
        error=replay_reference_find(replay,historical_ref,target,edge_endpoint);
        replay->history_owner=NULL;
    }
    if(historical_ref)ProReferenceFree(historical_ref);
    if(history){
        ProError cleanup=ProMdlErase(history);
        if(cleanup)return cleanup;
    }
    if(error)return error;
    part_text_add(replay->out,L"源特征 %d：从上一步恢复参照 %d，重用新件里已完成的特征。\n",
        replay->current_feature,id);
    return PRO_TK_NO_ERROR;
}

static ProError replay_reference_recover(ImitationReplay *replay,ProReference ref,ProModelitem *target,
    int edge_endpoint,ProError first)
{
    if((first!=PRO_TK_E_NOT_FOUND&&first!=PRO_TK_BAD_INPUTS)||
       replay->history_owner||!replay->last_feature||!replay->current_feature)return first;
    ProError retry=replay_reference_before_feature(replay,ref,target,edge_endpoint);
    return retry?first:PRO_TK_NO_ERROR;
}
static ProError replay_reference_item(ImitationReplay *replay,ProReference ref,ProModelitem *target,int edge_endpoint)
{
    ProError error=replay_reference_find(replay,ref,target,edge_endpoint);
    return replay_reference_recover(replay,ref,target,edge_endpoint,error);
}