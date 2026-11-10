static ProError sf_merge(FdTxn *tx,const PartCommand *cmd){
    FdRef quilt={0};
    ProElement array,entry;
    int ids[7],count=0;
    ProError e=model_element(tx->root,PRO_E_SRF_MRG_QUILT_ARR,0,0,&array);
    if(e)return e;
    for(int i=3;i<cmd->count;i++){
        if((e=sf_ref(tx->model,cmd->arg[i],&quilt)))goto done;
        for(int j=0;j<count;j++)if(ids[j]==quilt.item.id){e=PRO_TK_BAD_INPUTS;goto done;}
        ids[count++]=quilt.item.id;
        if((e=model_element(array,PRO_E_SRF_MRG_QUILT_CMPD,0,0,&entry))||
           (e=fd_ref_element(tx,entry,PRO_E_SRF_MRG_QUILT_REF,&quilt)))goto done;
        fd_ref_free(&quilt);memset(&quilt,0,sizeof(quilt));
    }
    e=model_element(tx->root,PRO_E_SRF_MRG_TYPE,1,PRO_SRF_MRG_JOIN,NULL);
done:
    fd_ref_free(&quilt);
    return e;
}
