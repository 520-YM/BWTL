static ProError model_remove_feature(ProSolid solid,int id){
    int *ids=NULL;ProFeatureDeleteOptions *opts=NULL;ProError error;
    error=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids);if(error)return error;ids[0]=id;
    error=ProArrayAlloc(1,sizeof(*opts),1,(ProArray *)&opts);
    if(!error){opts[0]=PRO_FEAT_DELETE_NO_OPTS;error=ProFeatureWithoptionsDelete(solid,ids,opts,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);}
    if(ids)ProArrayFree((ProArray *)&ids);if(opts)ProArrayFree((ProArray *)&opts);return error;
}
