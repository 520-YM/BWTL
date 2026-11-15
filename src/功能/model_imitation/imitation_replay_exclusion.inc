static ProError replay_exclusion_member(ProFeature *feature,int *member)
{
    ProParameter parameter;
    ProParamvalue value;
    ProError error=ProParameterInit(feature,L"BWTL_SKIP_MEMBER",&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,&value,NULL);
    if(!error&&(value.type!=PRO_PARAM_INTEGER||value.value.i_val<=0))error=PRO_TK_UNSUPPORTED;
    if(!error)*member=value.value.i_val;
    return error;
}

static ProError replay_exclusion(ImitationReplay *replay,ProFeature *source,ProFeature *created)
{
    int member=0,index=0,count=0;
    ProError error=replay_exclusion_member(source,&member);
    ProFeature old_member,new_member,header,*members=NULL;
    ProPattern pattern;
    if(!error)error=ProFeatureInit((ProSolid)replay->source,member,&old_member);
    ProPattern old_pattern;
    ProFeature old_header,old_mask;
    if(!error)error=ProFeaturePatternGet(&old_member,PRO_FEAT_PATTERN,&old_pattern);
    if(!error)error=ProPatternHeaderGet(&old_pattern,&old_header);
    if(!error&&(!ux_skip_mask(replay->source,old_header.id,member,&old_mask)||old_mask.id!=source->id))error=PRO_TK_INCOMPLETE;
    int mapped=0;
    for(int i=0;!error&&i<replay->count;i++)
        if(replay->mapped[i].source==member){mapped=replay->mapped[i].target;break;}
    if(!error&&!mapped)error=PRO_TK_E_NOT_FOUND;
    if(!error)error=ProFeatureInit((ProSolid)replay->target,mapped,&new_member);
    if(!error)error=ProFeaturePatternGet(&new_member,PRO_FEAT_PATTERN,&pattern);
    if(!error)error=ProPatternHeaderGet(&pattern,&header);
    if(!error)error=ProPatternMembersGet(&pattern,&members);
    if(!error)error=ProArraySizeGet((ProArray)members,&count);
    for(int i=0;!error&&i<count;i++)if(members[i].id==mapped){index=i+1;break;}
    if(!error&&index<2)error=PRO_TK_INCOMPLETE;
    ProName name;
    if(!error)error=ProModelitemNameGet(&header,name);
    ProMdlName model;
    if(!error)error=ProMdlMdlnameGet(replay->target,model);
    if(!error){
        wchar_t protocol[256],report[1024]=L"";
        PartCommand command;
        swprintf_s(protocol,_countof(protocol),L"CREO_OP REUSE_SKIP %ls %ls %d SKIP",model,name,index);
        if(part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
        else error=ux_skip(replay->target,&command,report,_countof(report));
    }
    if(!error&&!ux_skip_mask(replay->target,header.id,mapped,created))error=PRO_TK_INCOMPLETE;
    if(!error)part_text_add(replay->out,L"保留阵列第 %d 个实例的可恢复几何排除。\n",index);
    if(members)ProArrayFree((ProArray *)&members);
    return error;
}
