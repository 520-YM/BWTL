/* Reversible native geometry exclusion. Preserve the pattern and member IDs. */
static void ux_skip_name(int header,int member,ProName name){swprintf_s(name,PRO_NAME_SIZE,L"BWTL_SKIP_%d_%d",header,member);}
static int ux_skip_mask(ProMdl model,int header,int member,ProFeature *feature){ProName name;ProFeattype type;ProParameter param;ProParamvalue value;ux_skip_name(header,member,name);return !ux_named_feature(model,name,feature)&&!ProFeatureTypeGet(feature,&type)&&type==PRO_FEAT_RM_SURF&&!ProParameterInit((ProModelitem *)feature,L"BWTL_SKIP_MEMBER",&param)&&!ProParameterValueWithUnitsGet(&param,&value,NULL)&&value.type==PRO_PARAM_INTEGER&&value.value.i_val==member;}
static ProError ux_skip_regen(ProMdl model){ProError e;for(int i=0;i<3;i++){e=ProSolidRegenerate((ProSolid)model,PRO_REGEN_FORCE_REGEN|PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);if(e!=PRO_TK_REGEN_AGAIN)break;}return e;}
static ProError ux_skip(ProMdl model,const PartCommand *cmd,wchar_t *report,size_t capacity){
 ProFeature named,header,leader,*members=NULL,mask;ProPattern pattern;ProFeatStatus state;ProError e=0;double before,after,trial_volume=0;int count,index,skip=!wcscmp(cmd->arg[4],L"SKIP"),bodies=1,mutated=0;FdTxn tx={0};ProCollection collection=NULL;ProElement refs,el;ProName name;ProParameter marker;ProParamvalue value={0};ProMdl trial=NULL;ProPath backup=L"";ProMdlName trial_name=L"";
#define US(call) do{if((e=(call))){trace("reuse exclusion " #call,e);goto done;}}while(0)
 US(ux_solid(model,&before));US(ux_named_feature(model,cmd->arg[2],&named));US(ProFeaturePatternGet(&named,PRO_FEAT_PATTERN,&pattern));US(ProPatternHeaderGet(&pattern,&header));US(ProPatternLeaderGet(&pattern,&leader));US(ProPatternMembersGet(&pattern,&members));US(ProArraySizeGet((ProArray)members,&count));part_integer(cmd->arg[3],&index);index--;
 if(index>=count||members[index].id==leader.id){e=PRO_TK_BAD_INPUTS;goto done;}for(int i=0;i<count;i++){US(ProFeatureStatusGet(&members[i],&state));if(state!=PRO_FEAT_ACTIVE){e=PRO_TK_BAD_CONTEXT;goto done;}}
 int exists=ux_skip_mask(model,header.id,members[index].id,&mask);if(exists==skip){e=PRO_TK_BAD_CONTEXT;goto done;}ux_skip_name(header.id,members[index].id,name);
 if(skip){
  int *children=NULL,n=0;US(ProFeatureChildrenGet(&members[index],&children,&n));if(children)ProArrayFree((ProArray *)&children);if(n){e=PRO_TK_BAD_CONTEXT;goto done;}
  US(fd_begin(&tx,model,name,PRO_FEAT_RM_SURF));UxMirror surfaces={0};surfaces.tx=&tx;surfaces.n=1;surfaces.ids[0]=members[index].id;US(ProSrfcollectionAlloc(&collection));surfaces.collection=collection;US(ProSolidSurfaceVisit((ProSolid)model,ux_mirror_face,NULL,&surfaces));if(!surfaces.counts[0]){e=PRO_TK_E_NOT_FOUND;goto done;}
  US(model_element(tx.root,PRO_E_RM_SURF_REF_TYPE,1,PRO_RM_SURF_SRF_REF,NULL));US(model_element(tx.root,PRO_E_RM_SURF_ATTACH_TYPE,1,FM_RM_SURF_ATTACH_SAME,NULL));US(model_element(tx.root,PRO_E_RM_SURF_SRF_REFS,0,0,&refs));US(model_element(refs,PRO_E_STD_SURF_COLLECTION_APPL,0,0,&el));US(ProElementCollectionSet(el,collection));
  US(fd_create(&tx,0));mutated=1;mask=tx.feature;value.type=PRO_PARAM_INTEGER;value.value.i_val=members[index].id;US(ProParameterWithUnitsCreate((ProModelitem *)&mask,L"BWTL_SKIP_MEMBER",&value,NULL,&marker));
 }else{
  int *children=NULL,n=0;US(ProFeatureChildrenGet(&mask,&children,&n));if(children)ProArrayFree((ProArray *)&children);if(n){e=PRO_TK_BAD_CONTEXT;goto done;}
  /* Prove deletion on an isolated native recovery copy before changing source. */
  US(rf_delete_backup(model,&trial,backup,trial_name));US(model_remove_feature((ProSolid)trial,mask.id));US(ux_skip_regen(trial));US(ux_solid(trial,&trial_volume));if(part_close(before,trial_volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
  US(model_remove_feature((ProSolid)model,mask.id));mutated=1;
 }
 US(ux_skip_regen(model));US(ux_solid(model,&after));if(part_close(before,after)){e=PRO_TK_GENERAL_ERROR;goto done;}
 if(!skip&&!part_close(after,trial_volume)){e=PRO_TK_GENERAL_ERROR;goto done;}
 for(int i=0;i<count;i++){US(ProFeatureStatusGet(&members[i],&state));if(state!=PRO_FEAT_ACTIVE){e=PRO_TK_GENERAL_ERROR;goto done;}}
 swprintf_s(report,capacity,L"阵列 %ls 的第%d个实例已%ls。采用可逆的原生移除几何特征，保留阵列及实例ID；完整重生成、实体和材料变化已核实，未保存。",cmd->arg[2],index+1,skip?L"排除几何":L"恢复几何");
done:if(skip&&tx.root){am_finish(&tx,e,bodies,report,capacity);if(e&&!tx.rollback_ok)ux_recovery_uncertain=1;}else if(e&&mutated){ux_recovery_uncertain=1;swprintf_s(report,capacity,L"实例恢复未能核实，已停止后续复用写入。原生恢复副本位于%ls，名称%ls.prt；未保存当前件。",backup,trial_name);}if(trial){if(ProMdlErase(trial)||ux_copy_files_remove(trial_name))ux_recovery_uncertain=1;}if(collection)ProCollectionFree(&collection);if(members)ProArrayFree((ProArray *)&members);return e;
#undef US
}
