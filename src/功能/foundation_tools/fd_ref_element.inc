static ProError fd_ref_element(FdTxn *tx,ProElement parent,ProElemId id,const FdRef *ref){
    ProElement el=NULL;ProReference r=NULL;ProError e;int i;
    if(tx->ref_count>=32)return PRO_TK_BAD_INPUTS;
    if((e=model_element(parent,id,0,0,&el))||(e=ProSelectionToReference(ref->selection,&r)))return e;
    tx->refs[tx->ref_count++]=r;
    if((e=ProElementReferenceSet(el,r)))return e;
    for(i=0;i<tx->parent_count;i++)if(tx->parents[i]==ref->owner_feature)return 0;
    tx->parents[tx->parent_count++]=ref->owner_feature;return 0;
}
