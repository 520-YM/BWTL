static ProError aw_revisions(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxBomData data={0};
    ProError error=ax_bom_collect(assembly,&data,1);
    if(error)goto done;
    int correct=0,different=0,unknown=0,unlisted=0;
    for(int i=0;i<data.scan->count;i++){
        AxOccurrence *item=&data.scan->occurrences[i];
        if(ax_suppressed(item->state))continue;
        int index=data.rows[i+1];
        const AxBomModel *model=index>=0?&data.models[index]:NULL;
        wchar_t path[256];
        if(!ax_bom_path(item,path)){unknown++;continue;}
        const wchar_t *code=model?model->value[AB_CODE]:L"";
        const wchar_t *revision=model?model->value[AB_REV]:L"";
        if(!code[0]||!revision[0]){
            if(unknown++<50)part_text_add(out,L"%ls %ls 料号或版本未核实。\n",path,item->name);
            continue;
        }
        int j=2;
        while(j<cmd->count&&_wcsicmp(code,cmd->arg[j]))j+=2;
        if(j==cmd->count){
            if(unlisted++<50)part_text_add(out,L"%ls %ls（%ls）未列入批准清单。\n",path,code,revision);
        }else if(_wcsicmp(revision,cmd->arg[j+1])){
            if(different++<50)part_text_add(out,L"%ls %ls 当前 %ls，批准版本 %ls。\n",path,code,revision,cmd->arg[j+1]);
        }else correct++;
    }
    part_text_add(out,L"活动装配版本复核：符合 %d，版本不符 %d，未覆盖 %d，无法核实 %d。\n",correct,different,unlisted,unknown);
    if(different>50||unknown>50||unlisted>50)part_text_add(out,L"部分明细超出显示上限，统计包含所有读取到的实例。\n");
done:
    ax_bom_clear(&data);
    return error;
}
