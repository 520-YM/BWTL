static ProError sd_existing_body(FdTxn *tx,ProElement body){
    ProSolidBody *bodies=NULL;ProSelection sel=NULL;ProReference reference=NULL;ProElement el=NULL;ProError e;int n=0,i;
    e=ProSolidBodiesCollect((ProSolid)tx->model,&bodies);if(e)goto done;
    if((e=ProArraySizeGet((ProArray)bodies,&n)))goto done;e=PRO_TK_E_NOT_FOUND;
    for(i=0;i<n;i++){ProSolidBodyState state;if(ProSolidBodyStateGet(&bodies[i],&state)||state!=PRO_BODY_STATE_ACTIVE)continue;
        e=ProSelectionAlloc(NULL,(ProModelitem *)&bodies[i],&sel);break;
    }if(e)goto done;
    if((e=ProSelectionToReference(sel,&reference)))goto done;tx->refs[tx->ref_count++]=reference;
    if((e=model_element(body,PRO_E_BODY_SELECT,0,0,&el)))goto done;e=ProElementReferenceSet(el,reference);
done:if(bodies)ProArrayFree((ProArray *)&bodies);if(sel)ProSelectionFree(&sel);return e;
}
