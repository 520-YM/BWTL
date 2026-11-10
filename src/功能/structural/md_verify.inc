static ProError md_array_value(ProElement tree,const ProElemId *ids,int n,int index_at,int index,ProElement *element){
    ProElempath path=NULL;ProElempathItem p[8];ProError e;if(n>8)return PRO_TK_BAD_INPUTS;
    if((e=ProElempathAlloc(&path)))return e;
    for(int i=0;i<n;i++){p[i].type=i==index_at?PRO_ELEM_PATH_ITEM_TYPE_INDEX:PRO_ELEM_PATH_ITEM_TYPE_ID;p[i].path_item.elem_id=i==index_at?index:ids[i];}
    e=ProElempathDataSet(path,p,n);if(!e)e=ProElemtreeElementGet(tree,path,element);ProElempathFree(&path);return e;
}
static ProError md_verify_shell(FdTxn *tx,const PartCommand *cmd,int local){
    ProElement tree=NULL,element;ProReference *refs=NULL;ProError e;int count=0,id,wanted=local?1:cmd->count-4;
    if((e=mc_tree_value(&tx->feature,(ProElemId[]){PRO_E_SHELL_THICK},1,as_arg(cmd,3)*tx->scale,0))||
       (e=mc_tree_value(&tx->feature,(ProElemId[]){PRO_E_SHELL_FLIP},1,PRO_SHELL_INSIDE,1))||
       (e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||
       (e=mc_tree_element(tree,(ProElemId[]){PRO_E_SHELL_SRF},1,&element))||
       (e=ProElementReferencesGet(element,NULL,&refs))||(e=ProArraySizeGet((ProArray)refs,&count)))goto done;
    if(count!=wanted){e=PRO_TK_GENERAL_ERROR;goto done;}
    for(int i=0;i<wanted;i++){
        int expected,found=0;part_integer(cmd->arg[4+i]+8,&expected);
        for(int j=0;j<count;j++)if(!ProReferenceIdGet(refs[j],&id)&&id==expected)found=1;
        if(!found){e=PRO_TK_E_NOT_FOUND;goto done;}
        /* An open shell can retain the original surface ID for its rim. */
    }
    if(local)for(int i=5;i<cmd->count;i+=2){
        double thickness;ProReference reference=NULL;int expected;
        ProElemId path[]={PRO_E_ST_SHELL_LOCL_LIST,0,PRO_E_ST_SHELL_SPEC_THCK};
        if((e=md_array_value(tree,path,3,1,(i-5)/2,&element))||(e=ProElementDoubleGet(element,NULL,&thickness)))goto done;
        if(!part_close(thickness,as_arg(cmd,i+1)*tx->scale)){e=PRO_TK_GENERAL_ERROR;goto done;}
        path[2]=PRO_E_ST_SHELL_SPEC_SRF;
        if((e=md_array_value(tree,path,3,1,(i-5)/2,&element))||(e=ProElementReferenceGet(element,NULL,&reference)))goto done;
        e=ProReferenceIdGet(reference,&id);ProReferenceFree(reference);part_integer(cmd->arg[i]+8,&expected);
        if(e||id!=expected){if(!e)e=PRO_TK_GENERAL_ERROR;goto done;}
    }
done:if(refs)ProReferencearrayFree(refs);if(tree)ProFeatureElemtreeFree(&tx->feature,tree);return e;
}
static ProError md_verify_draft(FdTxn *tx,const PartCommand *cmd,int kind){
    int split=kind==MD_DRAFT_SPLIT,variable=kind==MD_DRAFT_VARIABLE;ProElement tree=NULL,element;ProError e;double angle;
    if((e=mc_tree_value(&tx->feature,(ProElemId[]){PRO_E_DRAFT_SPLIT},1,split?PRO_DRAFT_UI_SPLIT_NEUT:PRO_DRAFT_UI_SPLIT_NONE,1))||
       (e=mc_tree_value(&tx->feature,(ProElemId[]){PRO_E_DRAFT_CONSTANT_OR_VARIABLE},1,variable?PRO_DRAFT_UI_VARIABLE:PRO_DRAFT_UI_CONSTANT,1)))return e;
    if(variable){
        if((e=ProFeatureElemtreeExtract(&tx->feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)))return e;
        for(int i=0;i<2;i++){
            ProElemId path[]={PRO_E_DRAFT_SIDE_1,PRO_E_DRAFT_ANGLES,0,PRO_E_DRAFT_ANGLE};
            if((e=md_array_value(tree,path,4,2,i,&element))||(e=ProElementDoubleGet(element,NULL,&angle)))goto done;
            if(!part_close(angle,as_arg(cmd,7+2*i))){e=PRO_TK_GENERAL_ERROR;goto done;}
        }
    }else for(int i=0;i<(split?2:1);i++){
        ProElemId path[]={i?PRO_E_DRAFT_SIDE_2:PRO_E_DRAFT_SIDE_1,i?PRO_E_DRAFT_ANGLE_2:PRO_E_DRAFT_ANGLE_1};
        if((e=mc_tree_value(&tx->feature,path,2,as_arg(cmd,5+i),0)))goto done;
    }
    for(int i=variable?3:split?8:7;i<(variable?4:cmd->count);i++){
        FdRef expected={0};int id;part_integer(cmd->arg[i]+8,&id);
        if((e=ProModelitemInit(tx->model,id,PRO_SURFACE,&expected.item))||(e=sd_projection_reference(tx->model,&tx->feature,&expected)))goto done;
    }
done:if(tree)ProFeatureElemtreeFree(&tx->feature,tree);return e;
}
