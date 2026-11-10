static ProError md_solid_face(FdTxn *tx,const wchar_t *token,FdRef *ref){
    ProError e=fd_ref_resolve(tx->model,token,PRO_SURFACE,ref);
    if(e)return e;e=ProSolidSurfaceVisit((ProSolid)tx->model,mc_find_shell_face,NULL,&ref->item.id);
    return e==PRO_TK_E_FOUND?0:e?e:PRO_TK_BAD_INPUTS;
}
static ProError md_multiple_refs(FdTxn *tx,ProElement parent,ProElemId id,FdRef *faces,int count){
    ProReference *array=NULL;ProElement element;ProError e;
    if((e=ProArrayAlloc(count,sizeof(ProReference),1,(ProArray *)&array)))return e;
    for(int i=0;i<count;i++){
        if(tx->ref_count>=_countof(tx->refs)){e=PRO_TK_OUT_OF_RANGE;goto done;}
        if((e=ProSelectionToReference(faces[i].selection,&array[i])))goto done;
        tx->refs[tx->ref_count++]=array[i];
        int j;for(j=0;j<tx->parent_count&&tx->parents[j]!=faces[i].owner_feature;j++){}
        if(j==tx->parent_count)tx->parents[tx->parent_count++]=faces[i].owner_feature;
    }
    if((e=model_element(parent,id,0,0,&element)))goto done;
    e=ProElementReferencesSet(element,array);
done:if(array)ProArrayFree((ProArray *)&array);return e;
}
static ProError md_shell(FdTxn *tx,const PartCommand *cmd,int local){
    FdRef faces[8]={{0}},special[5]={{0}};ProElement body,list,entry;int count=local?1:cmd->count-4;ProError e;
#define MS(call) do{if((e=(call)))goto done;}while(0)
    for(int i=0;i<count;i++)MS(md_solid_face(tx,cmd->arg[4+i],&faces[i]));
    MS(model_element(tx->root,PRO_E_BODY,0,0,&body));MS(model_element(body,PRO_E_BODY_USE,1,PRO_BODY_USE_SELECTED,NULL));
    MS(sd_existing_body(tx,body));MS(md_multiple_refs(tx,tx->root,PRO_E_SHELL_SRF,faces,count));
    MS(model_element(tx->root,PRO_E_SHELL_THICK,2,as_arg(cmd,3)*tx->scale,NULL));
    MS(model_element(tx->root,PRO_E_SHELL_FLIP,1,PRO_SHELL_INSIDE,NULL));
    if(local){
        MS(model_element(tx->root,PRO_E_ST_SHELL_LOCL_LIST,0,0,&list));
        for(int i=5;i<cmd->count;i+=2){
            FdRef *ref=&special[(i-5)/2];MS(md_solid_face(tx,cmd->arg[i],ref));
            if(ref->item.id==faces[0].item.id){e=PRO_TK_BAD_INPUTS;goto done;}
            for(int j=0;j<(i-5)/2;j++)if(special[j].item.id==ref->item.id){e=PRO_TK_BAD_INPUTS;goto done;}
            MS(model_element(list,PRO_E_ST_SHELL_LOCL_CMPD,0,0,&entry));
            MS(fd_ref_element(tx,entry,PRO_E_ST_SHELL_SPEC_SRF,ref));
            MS(model_element(entry,PRO_E_ST_SHELL_SPEC_THCK,2,as_arg(cmd,i+1)*tx->scale,NULL));
        }
    }
    MS(fd_create(tx,0));
done:for(int i=0;i<8;i++)fd_ref_free(&faces[i]);for(int i=0;i<5;i++)fd_ref_free(&special[i]);return e;
#undef MS
}
