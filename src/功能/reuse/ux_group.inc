static ProError ux_group(FdTxn *tx,const PartCommand *cmd){
 int *ids=NULL,n=cmd->count-3,numbers[8];ProGroup group={0};ProFeature *members=NULL;ProError e;int created=0,actual;
#define UG(call) do{if((e=(call)))goto done;}while(0)
 UG(ProArrayAlloc(n,sizeof(int),1,(ProArray *)&ids));
 for(int i=0;i<n;i++){ProFeature f;ProFeattype type;ProGroupStatus status;ProPatternStatus ps;UG(rf_active_feature(tx->model,cmd->arg[i+3],&f,&type));UG(ProFeatureGroupStatusGet(&f,&status));UG(ProFeaturePatternStatusGet(&f,&ps));if(status!=PRO_GROUP_NONE||ps!=PRO_PATTERN_NONE){e=PRO_TK_BAD_CONTEXT;goto done;}ids[i]=f.id;UG(ProFeatureNumberGet(&f,&numbers[i]));if(i&&numbers[i]!=numbers[i-1]+1){e=PRO_TK_BAD_INPUTS;goto done;}}
 UG(ProLocalGroupCreate((ProSolid)tx->model,ids,n,tx->name,&group));created=1;UG(ProGroupFeaturesCollect(&group,&members));UG(ProArraySizeGet((ProArray)members,&actual));
 for(int i=0;i<n;i++){int found=0;for(int j=0;j<actual;j++)if(members[j].id==ids[i])found++;if(found!=1){e=PRO_TK_GENERAL_ERROR;goto done;}}
 tx->feature=group;UG(part_regenerate(tx->model));
done:if(e&&created&&(ProGroupUngroup(&group)||part_regenerate(tx->model)))tx->rollback_ok=0;if(ids)ProArrayFree((ProArray *)&ids);if(members)ProArrayFree((ProArray *)&members);return e;
#undef UG
}
