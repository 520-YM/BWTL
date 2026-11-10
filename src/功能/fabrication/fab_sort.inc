static ProError fab_parameter_leaf(ProModelitem *owner,const wchar_t *key,ProName result){
    ProParameter parameter;
    ProParamvalue value;
    ProError error=ProParameterInit(owner,(wchar_t *)key,&parameter);
    if(!error)error=ProParameterValueWithUnitsGet(&parameter,&value,NULL);
    if(error)return error;
    if(value.type!=PRO_PARAM_STRING||wcslen(value.value.s_val)>=PRO_NAME_SIZE||!dv_safe_leaf(value.value.s_val))return PRO_TK_BAD_INPUTS;
    return wcscpy_s(result,PRO_NAME_SIZE,value.value.s_val)?PRO_TK_BAD_INPUTS:PRO_TK_NO_ERROR;
}

static ProError fab_sort(ProMdl model,const PartCommand *cmd,PartText *out){
    ProMdl models[6]={model};
    ProName codes[6],versions[6];
    ProPath groups[6];
    const int count=cmd->count-2;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<count;i++){
        ProModelitem owner;
        if(i)error=ProMdlnameInit((wchar_t *)cmd->arg[2+i],PRO_MDL_PART,&models[i]);
        if(!error)error=part_health(models[i]);
        if(!error)error=qa_single_body(models[i],NULL);
        if(!error)error=ProMdlToModelitem(models[i],&owner);
        if(!error){
            error=fab_parameter_leaf(&owner,L"PART_CODE",codes[i]);
            if(error==PRO_TK_E_NOT_FOUND)error=fab_parameter_leaf(&owner,L"DRAWING_NO",codes[i]);
        }
        if(!error)error=fab_parameter_leaf(&owner,L"REVISION",versions[i]);
        if(!error&&(swprintf_s(groups[i],PRO_PATH_SIZE,L"%ls__%ls",codes[i],versions[i])<0||!dv_safe_leaf(groups[i])))
            error=PRO_TK_BAD_INPUTS;
        for(int j=0;!error&&j<i;j++)if(!_wcsicmp(groups[i],groups[j]))error=PRO_TK_E_FOUND;
        if(error)part_text_add(out,L"请核对零件 %ls 的料号和版本。必须有可作为目录名的字符串，并且组合唯一。\n",cmd->arg[i?i+2:1]);
    }
    DeliveryOutput work={0},children[6]={0};
    FILE *index=NULL;
    if(!error)error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=dv_stage_enter(&work);
    if(!error)error=fab_csv_open(work.stage,L"release-index.csv",&index);
    if(!error)fwprintf(index,L"模型,料号,版本,相对目录,STEP文件,SHA256,字节\n");
    for(int i=0;!error&&i<count;i++){
        ProPath path,leaf;
        wchar_t hash[65];
        ULONGLONG bytes;
        int features,bores;
        DeliveryOutput *child=&children[i];
        error=fab_leaf_path(work.stage,groups[i],L"",child->stage);
        if(!error&&!CreateDirectoryW(child->stage,NULL))error=PRO_TK_CANT_WRITE;
        if(error)break;
        child->owned=1;
        error=ProDirectoryChange(child->stage);
        if(!error)error=dv_export_file(models[i],child->stage,PRO_INTF_EXPORT_STEP,leaf);
        if(!error)error=dv_report_file(models[i],child->stage,&features,&bores);
        if(!error)error=fab_leaf_path(child->stage,leaf,L"",path);
        if(!error&&!dv_sha256(path,hash,&bytes))error=PRO_TK_CANT_OPEN;
        if(!error){
            const wchar_t *columns[]={cmd->arg[i?i+2:1],codes[i],versions[i],groups[i],leaf,hash};
            for(int j=0;j<_countof(columns);j++){
                if(!dv_csv_text(index,columns[j])||fputwc(L',',index)==WEOF)error=PRO_TK_CANT_WRITE;
            }
            fwprintf(index,L"%llu\n",bytes);
        }
    }
    error=fab_close(index,error);
    if(work.in_stage){
        ProError restore=ProDirectoryChange(work.parent);
        if(restore){
            dv_recovery_uncertain=1;
            part_text_add(out,L"工作目录恢复未能核实，临时输出位于 %ls。\n",work.stage);
            return restore;
        }
        work.in_stage=0;
    }
    if(work.owned&&!error){
        if(MoveFileExW(work.stage,work.target,MOVEFILE_WRITE_THROUGH))work.owned=0;
        else error=PRO_TK_CANT_WRITE;
    }
    if(error)for(int i=0;i<count;i++)if(children[i].owned&&!dv_stage_clean(&children[i]))
        part_text_add(out,L"保留待检查的临时目录 %ls。\n",children[i].stage);
    error=fab_discard(&work,error,out);
    if(!error)part_text_add(out,L"已按料号和版本整理 %d 套 STEP 与加工信息表。\n%ls\\release-index.csv",count,work.target);
    return error;
}
