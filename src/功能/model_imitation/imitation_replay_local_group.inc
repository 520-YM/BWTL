static ProError replay_local_group_read(ProFeature *feature,ReplayLocalGroup *group)
{
    ProName udf=L"",instance=L"";
    ProError error=ProModelitemNameGet(feature,group->name);
    if(error)return error;
    if(!ProUdfNameGet((ProGroup *)feature,udf,instance)&&
       (instance[0]||(udf[0]&&_wcsicmp(udf,group->name))))return PRO_TK_UNSUPPORTED;
    ProBoolean tabled=PRO_B_FALSE;
    if(!ProGroupIsTabledriven((ProGroup *)feature,&tabled)&&tabled)return PRO_TK_UNSUPPORTED;
    ProPatternStatus pattern;
    error=ProFeaturePatternStatusGet(feature,&pattern);
    ProGrppatternStatus group_pattern;
    if(!error)error=ProFeatureGrppatternStatusGet(feature,&group_pattern);
    if(!error&&pattern!=PRO_PATTERN_NONE&&
       (pattern!=PRO_PATTERN_LEADER||group_pattern!=PRO_GRP_PATTERN_LEADER))error=PRO_TK_UNSUPPORTED;
    if(!error)group->patterned=group_pattern==PRO_GRP_PATTERN_LEADER;
    ProFeature *items=NULL;
    if(!error)error=ProGroupFeaturesCollect((ProGroup *)feature,&items);
    int size=0;
    if(!error)error=ProArraySizeGet((ProArray)items,&size);
    if(!error&&size>REPLAY_FEATURE_MAX)error=PRO_TK_OUT_OF_RANGE;
    for(int i=0;!error&&i<size;i++){
        if(items[i].id==feature->id)continue;
        ProFeattype type;
        ProGroup parent;
        ProBoolean visible=PRO_B_FALSE,embedded=PRO_B_FALSE;
        error=ProFeatureTypeGet(&items[i],&type);
        if(!error)error=ProFeatureVisibilityGet(&items[i],&visible);
        if(!error)error=ProFeatureIsEmbedded(&items[i],&embedded);
        if(!error)error=ProFeaturePatternStatusGet(&items[i],&pattern);
        if(!error)error=ProFeatureGroupGet(&items[i],&parent);
        if(!error&&(type==PRO_FEAT_DERIVED_MEMBER||type==PRO_FEAT_GROUP_HEAD||
                    !visible||embedded||pattern!=PRO_PATTERN_NONE||parent.id!=feature->id))
            error=PRO_TK_UNSUPPORTED;
        if(!error)group->members[group->count++]=items[i].id;
    }
    if(items)ProArrayFree((ProArray *)&items);
    if(!error&&group->count<2)error=PRO_TK_UNSUPPORTED;
    if(!error)group->id=feature->id;
    return error;
}

static ProError replay_local_group_finish(ImitationReplay *replay)
{
    ReplayLocalGroup *group=&replay->local_group;
    int *ids=NULL;
    ProFeature *members=NULL;
    ProGroup created;
    ProError error=ProArrayAlloc(group->count,sizeof(*ids),1,(ProArray *)&ids);
    for(int i=0;!error&&i<group->count;i++){
        ids[i]=0;
        for(int j=0;j<replay->count;j++)if(replay->mapped[j].source==group->members[i]){
            ids[i]=replay->mapped[j].target;
            break;
        }
        if(!ids[i])error=PRO_TK_E_NOT_FOUND;
    }
    if(!error)error=ProLocalGroupCreate((ProSolid)replay->target,ids,group->count,group->name,&created);
    if(!error)error=ProGroupFeaturesCollect(&created,&members);
    int total=0,matched=0;
    if(!error)error=ProArraySizeGet((ProArray)members,&total);
    for(int i=0;!error&&i<total;i++){
        if(members[i].id==created.id)continue;
        if(matched==group->count||members[i].id!=ids[matched])error=PRO_TK_INCOMPLETE;
        else matched++;
    }
    if(!error&&matched!=group->count)error=PRO_TK_INCOMPLETE;
    if(!error)error=part_regenerate(replay->target);
    if(!error)error=replay_add_mapping(replay,group->id,created.id);
    if(!error&&group->patterned){
        ProFeature source,seed;
        error=ProFeatureInit((ProSolid)replay->source,group->id,&source);
        if(!error)error=ProFeatureInit((ProSolid)replay->target,created.id,&seed);
        if(!error)error=replay_pattern(replay,&source,&seed,PRO_GROUP_PATTERN);
    }
    if(error){
        part_text_add(replay->out,L"原组 %ls ID %d，重建未完成，Creo %d。\n",group->name,group->id,error);
        if(replay->failure){
            replay->failure->step=group->step;
            replay->failure->feature=group->id;
            replay->failure->operation=group->patterned?L"本地特征组阵列":L"本地特征组";
            wcscpy_s(replay->failure->label,_countof(replay->failure->label),group->name);
        }
    }else part_text_add(replay->out,L"原组 %ls ID %d → 新组 ID %d，按原顺序核对 %d 个成员，Creo 0。\n",
        group->name,group->id,created.id,matched);
    if(members)ProArrayFree((ProArray *)&members);
    if(ids)ProArrayFree((ProArray *)&ids);
    *group=(ReplayLocalGroup){0};
    return error;
}
