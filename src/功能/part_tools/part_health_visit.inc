static ProError part_health_visit(ProFeature *f,ProError status,ProAppData data){
    unsigned flags;ProError e;if(status)return status;
    if((e=ProFeatureStatusflagsGet(f,&flags)))return e;
    return flags&(PRO_FEAT_STAT_FAILED|PRO_FEAT_STAT_CHILD_OF_FAILED|PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)?PRO_TK_GENERAL_ERROR:0;
}
