static ProSelection aw_highlighted;

static ProError aw_highlight(ProMdl assembly,const PartCommand *cmd,PartText *out){
    if(aw_highlighted){
        ProSelectionUnhighlight(aw_highlighted);
        ProSelectionFree(&aw_highlighted);
    }
    if(!wcscmp(cmd->arg[2],L"CLEAR")){
        part_text_add(out,L"已清除装配检查高亮。\n");return PRO_TK_NO_ERROR;
    }
    AxComponent component={0};
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&component);
    if(error)return error;
    ProSelection selection=NULL;
    if(cmd->count==4){
        FdRef ref={0};
        error=fd_ref_resolve(component.model,cmd->arg[3],PRO_SURFACE,&ref);
        if(!error)error=ProSelectionAlloc(&component.path,&ref.item,&selection);
        fd_ref_free(&ref);
    }else{
        ProModelitem item;
        error=ProMdlToModelitem(component.model,&item);
        if(!error)error=ProSelectionAlloc(&component.path,&item,&selection);
    }
    if(!error)error=ProSelectionHighlight(selection,PRO_COLOR_HIGHLITE);
    if(error){
        if(selection)ProSelectionFree(&selection);
        return error;
    }
    aw_highlighted=selection;
    part_text_add(out,L"已在活动模型高亮 %ls %ls。此操作只改变显示。\n",cmd->arg[2],cmd->count==4?cmd->arg[3]:L"组件");
    return PRO_TK_NO_ERROR;
}

static ProError aw_weld_check(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    int failed=0,unverified=0;
    for(int i=2;i<cmd->count;i+=3){
        AxComponent weld={0},mate={0};
        ProSelection items[2]={0};
        ProError error=ax_hw_occurrence(assembly,cmd->arg[i],mm,&weld);
        if(!error)error=ax_hw_occurrence(assembly,cmd->arg[i+1],mm,&mate);
        if(!error)error=ax_part_selection(&weld,&items[0]);
        if(!error)error=ax_part_selection(&mate,&items[1]);
        double volume=0;
        if(!error)error=ax_interference_volume(items[0],items[1],&volume);
        for(int j=0;j<2;j++)if(items[j])ProSelectionFree(&items[j]);
        if(error){
            unverified++;
            part_text_add(out,L"焊缝包络 %ls 和后装件 %ls 未核实（Creo %d）。\n",cmd->arg[i],cmd->arg[i+1],error);
            continue;
        }
        volume*=mm*mm*mm;
        const double allowed=as_arg(cmd,i+2);
        int pass=ax_q_within(volume,allowed);
        failed+=!pass;
        part_text_add(out,L"焊缝包络 %ls 与后装件 %ls 相交 %.8g mm³（允许 ≤ %.8g），%ls。\n",cmd->arg[i],cmd->arg[i+1],
            volume,allowed,pass?L"符合":L"焊缝包络影响装配");
    }
    part_text_add(out,L"查指定已建焊缝包络：影响装配 %d 对，未核实 %d 对。\n",failed,unverified);
    return unverified?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
