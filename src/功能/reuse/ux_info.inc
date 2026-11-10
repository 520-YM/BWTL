static ProError ux_info_pattern(ProFeature *feature,ProError status,ProAppData data){
 PartText *text=data;ProPatternStatus ps;ProPattern pattern;ProFeature header,leader,f,parent;ProName name;int *ids=NULL,total=0,index=0;ProFeatStatus *states=NULL;ProError e=0;
 if(status)return status;if(ProFeaturePatternStatusGet(feature,&ps)||ps!=PRO_PATTERN_LEADER)return 0;
 if(ProFeaturePatternGet(feature,PRO_FEAT_PATTERN,&pattern)||ProPatternHeaderGet(&pattern,&header)||ProPatternLeaderGet(&pattern,&leader))return 0;
 if(ProModelitemNameGet((ProModelitem *)&header,name))return 0;part_text_add(text,L"\n阵列 %ls：真实重生成顺序，序号1为种子。\n",name);
 if((e=ProArrayAlloc(0,sizeof(int),1,(ProArray *)&ids))||(e=ProArrayAlloc(0,sizeof(ProFeatStatus),1,(ProArray *)&states))||(e=ProSolidFeatstatusGet((ProSolid)feature->owner,&ids,&states,&total)))goto done;
 for(int i=0;i<total;i++){if(ids[i]==header.id||ProFeatureInit((ProSolid)feature->owner,ids[i],&f))continue;if(ids[i]!=leader.id&&(ProFeaturePatternleaderGet(&f,&parent)||parent.id!=leader.id))continue;part_text_add(text,L"实例%d：特征ID=%d，状态=%d%ls\n",++index,ids[i],states[i],ux_skip_mask(feature->owner,header.id,ids[i],&parent)?L"（已排除几何，原阵列保留）":L"");}
done:if(ids)ProArrayFree((ProArray *)&ids);if(states)ProArrayFree((ProArray *)&states);return e;
}
static ProError ux_info(ProMdl model,wchar_t *report,size_t capacity){
 PartText text={report,capacity,0};PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));ProError e;if(!snapshot)return PRO_TK_OUT_OF_MEMORY;
 e=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,part_dimension_visit,NULL,snapshot);if(e==PRO_TK_E_NOT_FOUND)e=0;
 if(!e){part_text_add(&text,L"当前模型实际驱动尺寸（值为模型原生单位；建模前核对单位）：\n");for(int i=0;i<snapshot->dimensions;i++){PartDimension *d=&snapshot->dimension[i];part_text_add(&text,L"%ls=%.12g，特征%d，类型%d，关系驱动%d\n",d->symbol,d->value,d->feature,d->type,d->related);}e=ProSolidFeatVisit((ProSolid)model,ux_info_pattern,NULL,&text);if(e==PRO_TK_E_NOT_FOUND)e=0;wchar_t refs[20000];if(!foundation_references(model,refs,_countof(refs)))part_text_add(&text,L"%ls",refs);}
 free(snapshot);return e;
}
