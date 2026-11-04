static ProError fd_sketch_open(ProMdl model,const wchar_t *name,FdSketch *s,int backup){
    ProModelitem item;ProFeattype type;ProFeatStatus status;ProError e;
    memset(s,0,sizeof(*s));
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,&item)))return e;s->feature=*(ProFeature *)&item;
    if((e=ProFeatureTypeGet(&s->feature,&type))||(e=ProFeatureStatusGet(&s->feature,&status)))return e;
    if(type!=PRO_FEAT_CURVE||status!=PRO_FEAT_ACTIVE)return PRO_TK_INVALID_TYPE;
    if((e=ProFeatureElemtreeExtract(&s->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s->tree))||(e=fd_section_from_tree(s->tree,&s->section)))return e;
    if(backup){
        if((e=ProFeatureElemtreeExtract(&s->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&s->backup))||(e=fd_section_from_tree(s->backup,&s->original)))return e;
        if(s->section==s->original)return PRO_TK_BAD_CONTEXT;s->editing=1;
    }
    return ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&s->options);
}
