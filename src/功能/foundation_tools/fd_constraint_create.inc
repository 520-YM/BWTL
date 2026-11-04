static ProError fd_constraint_create(ProSection section,ProConstraintType kind,int n,int *ids,ProSectionPointType *senses,int *created){
    ProSelection *sels=NULL;int i,cid=-1;ProBoolean im=PRO_B_FALSE;ProError e;
    if(n<1||n>3)return PRO_TK_BAD_INPUTS;
    *created=0;
    if((e=ProArrayAlloc(n,sizeof(ProSelection),1,(ProArray *)&sels)))return e;
    memset(sels,0,n*sizeof(ProSelection));
    if((e=ProSectionIntentManagerModeGet(section,&im)))goto done;
    if(!im&&(e=ProSectionIntentManagerModeSet(section,PRO_B_TRUE))){trace("foundation enable intent manager",e);goto done;}
    for(i=0;i<n;i++){
        e=ProSectionEntityGetSelected(section,ids[i],senses[i],NULL,PRO_VALUE_UNUSED,&sels[i]);if(e)goto done;
    }
    e=ProSectionConstraintCreate(section,sels,n,kind,&cid);trace("foundation constraint create",e);
    /* Keep the owned section in Intent Manager mode. Converting back here
       discards newly introduced constraints before the feature is resolved. */
    if(e&&e!=PRO_TK_E_FOUND)goto done;
    e=fd_constraint_evidence(section,kind,n,ids,senses,created);
done:
    if(sels){for(i=0;i<n;i++)if(sels[i])ProSelectionFree(&sels[i]);ProArrayFree((ProArray *)&sels);}return e;
}
