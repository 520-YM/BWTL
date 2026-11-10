static int dv_datum_type(ProFeattype type){
    return type==PRO_FEAT_DATUM||type==PRO_FEAT_DATUM_AXIS||type==PRO_FEAT_DATUM_POINT||
        type==PRO_FEAT_CSYS||type==PRO_FEAT_CURVE;
}

static ProError dv_check_order(ProMdl model,ProFeature *feature,int old_number,int new_number,int parents){
    int *ids=NULL,count=0;
    ProError error=parents?ProFeatureParentsGet(feature,&ids,&count):ProFeatureChildrenGet(feature,&ids,&count);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++){
        ProFeature related;
        int number;
        if((error=ProFeatureInit((ProSolid)model,ids[i],&related))||
           (error=ProFeatureNumberGet(&related,&number)))break;
        if(number>old_number)number--;
        if(number>=new_number)number++;
        if(parents?number>=new_number:number<=new_number)error=PRO_TK_BAD_CONTEXT;
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    return error;
}

static ProError dv_reorder(ProMdl model,const PartCommand *cmd,PartText *out){
    ProFeature feature,anchor;
    ProFeattype type;
    ProFeatStatus state;
    ProExtRefInfo *external=NULL;
    int original,target,anchor_number,actual,external_count=0;
    int *ids=NULL;
    ProError error;
    if((error=dv_feature(model,cmd->arg[2],&feature,&original))||
       (error=dv_feature(model,cmd->arg[4],&anchor,&anchor_number))||
       (error=ProFeatureTypeGet(&feature,&type))||(error=ProFeatureStatusGet(&feature,&state)))goto done;
    if(!dv_datum_type(type)||state!=PRO_FEAT_ACTIVE){error=PRO_TK_BAD_CONTEXT;goto done;}
    error=ProFeatureExternParentsGet(&feature,PRO_ALL_EXT_REF_TYPES,&external,&external_count);
    if(error&&error!=PRO_TK_E_NOT_FOUND)goto done;
    if(external_count){error=PRO_TK_BAD_CONTEXT;goto done;}
    int after=!wcscmp(cmd->arg[3],L"AFTER");
    target=anchor_number+after-(original<anchor_number);
    if(target==original){error=PRO_TK_NO_CHANGE;goto done;}
    if((error=dv_check_order(model,&feature,original,target,1))||
       (error=dv_check_order(model,&feature,original,target,0))||
       (error=ProArrayAlloc(1,sizeof(int),1,(ProArray *)&ids)))goto done;
    ids[0]=feature.id;
    error=ProFeatureWithoptionsReorder((ProSolid)model,ids,target,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL);
    if(!error)error=part_health(model);
    if(!error)error=ProFeatureNumberGet(&feature,&actual);
    if(!error)error=ProFeatureNumberGet(&anchor,&anchor_number);
    if(!error&&(actual!=target||actual!=anchor_number+(after?1:-1)))error=PRO_TK_GENERAL_ERROR;
    if(error){
        ProError read_error=ProFeatureNumberGet(&feature,&actual);
        if(read_error||(actual!=original&&ProFeatureWithoptionsReorder((ProSolid)model,ids,original,
           PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL))||
           ProFeatureNumberGet(&feature,&actual)||actual!=original||part_health(model)){
            dv_recovery_uncertain=1;
            part_text_add(out,L"原顺序恢复未能核实，本次会话已停止交付模块写入。\n");
        }
    }else part_text_add(out,L"已将 %ls 移到 %ls %ls，重生成序号 %d → %d。已核对父子依赖和实际顺序。\n",
        cmd->arg[2],cmd->arg[4],after?L"之后":L"之前",original,actual);
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    if(external)ProExtRefInfoFree(&external);
    return error;
}
