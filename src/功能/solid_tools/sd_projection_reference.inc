static ProError sd_projection_reference(ProMdl model,ProFeature *feature,const FdRef *expected){
    ProReference *refs=NULL;int count=0,i,found=0;ProError e;
    e=ProFeatureReferenceEditRefsGet((ProSolid)model,feature,PRO_EDITREF_REF_TYPE_ALL,&refs);if(e)return e;
    e=ProArraySizeGet((ProArray)refs,&count);if(e)goto done;
    for(i=0;i<count;i++){ProSelection selection=NULL;ProModelitem item;
        if(!ProReferenceToSelection(refs[i],&selection)&&!ProSelectionModelitemGet(selection,&item)&&item.id==expected->item.id&&item.type==expected->item.type&&item.owner==model)found=1;
        if(selection)ProSelectionFree(&selection);if(found)break;
    }e=found?0:PRO_TK_E_NOT_FOUND;
done:if(refs)ProReferencearrayFree(refs);return e;
}
