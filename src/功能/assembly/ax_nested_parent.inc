/* Mutating a child assembly changes its model definition. Refuse ambiguous
   repeated parent occurrences when the request names just one occurrence. */
static ProError ax_nested_parent(ProMdl root,const wchar_t *token,ProMdl *parent,PartText *out){
    AxComponent target={0};
    ProError error=ax_component_resolve(root,token,&target);
    if(error){
        part_text_add(out,L"找不到父装配实例 %ls。请先读取实际组件树。\n",token);
        return error;
    }
    ProMdlType type;
    error=ProMdlTypeGet(target.model,&type);
    if(!error&&type!=PRO_MDL_ASSEMBLY)error=PRO_TK_INVALID_TYPE;
    if(!error)error=ax_master(target.model);
    if(!error)error=part_health(target.model);
    if(error){
        part_text_add(out,L"%ls 不是可写的活动父装配。请核对主表示和重生成状态。\n",token);
        return error;
    }
    AxScan *scan=NULL;
    int occurrences=0;
    error=ax_scan(root,1,&scan);
    if(!error){
        for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model==target.model)++occurrences;
    }
    free(scan);
    if(error)return error;
    if(occurrences!=1){
        part_text_add(out,L"父装配 %ls 的模型定义有 %d 个实例。请明确使用哪个独立装配模型以避免一处修改联动到其他位置。\n",
            token,occurrences);
        return PRO_TK_BAD_CONTEXT;
    }
    *parent=target.model;
    return PRO_TK_NO_ERROR;
}