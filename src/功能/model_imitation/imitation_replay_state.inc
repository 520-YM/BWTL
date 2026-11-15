static const wchar_t *replay_feature_issue(ProFeatStatus state,unsigned int flags){
    if(flags&PRO_FEAT_STAT_FAILED)return L"再生失败";
    if(flags&PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)return L"外部父特征再生失败";
    if(flags&PRO_FEAT_STAT_CHILD_OF_FAILED)return L"父特征再生失败";
    switch(state){
    case PRO_FEAT_ACTIVE:
        return flags&PRO_FEAT_STAT_UNREGENERATED?L"尚未再生":NULL;
    case PRO_FEAT_FAMTAB_SUPPRESSED:return L"族表抑制";
    case PRO_FEAT_SIMP_REP_SUPPRESSED:return L"简化表示抑制";
    case PRO_FEAT_PROG_SUPPRESSED:return L"程序抑制";
    case PRO_FEAT_SUPPRESSED:return L"手动抑制";
    case PRO_FEAT_INACTIVE:return L"未激活";
    case PRO_FEAT_UNREGENERATED:return L"尚未再生";
    default:return L"特征状态无效";
    }
}

static ProError replay_feature_state(ProFeature *feature,ProFeatStatus *state,
                                     unsigned int *flags,const wchar_t **issue){
    ProError error=ProFeatureStatusGet(feature,state);
    if(!error)error=ProFeatureStatusflagsGet(feature,flags);
    if(!error)*issue=replay_feature_issue(*state,*flags);
    return error;
}

static ProError replay_find_failed_feature(ProFeature *feature,ProError status,ProAppData data)
{
    if(status)return status;
    ProFeatStatus state;
    unsigned int flags;
    const wchar_t *issue=NULL;
    ProError error=replay_feature_state(feature,&state,&flags,&issue);
    if(error)return error;
    if(!(flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)))
        return PRO_TK_NO_ERROR;
    ReplayFailure *failure=data;
    failure->feature=feature->id;
    failure->operation=issue;
    ProModelitemNameGet(feature,failure->label);
    return PRO_TK_E_FOUND;
}

