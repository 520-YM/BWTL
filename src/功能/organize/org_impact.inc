static ProError org_impact(const OrganizeView *view,const PartCommand *cmd,PartText *out){
    ProFeature feature={0};
    ProName name;
    ProFeatStatus state;
    unsigned flags;
    int *children=NULL,count=0;
    ProError error=org_feature(view->model,cmd->arg[2],&feature);
    if(!error)error=org_read_status(&feature,&state,&flags);
    if(error)return error;
    error=ProFeatureChildrenGet(&feature,&children,&count);
    if(children)ProArrayFree((ProArray *)&children);
    if(error&&error!=PRO_TK_E_NOT_FOUND)return error;
    qa_feature_label(&feature,name);
    const wchar_t *action=!wcscmp(cmd->arg[3],L"DELETE")?L"删除":!wcscmp(cmd->arg[3],L"SUPPRESS")?L"抑制":L"修改";
    part_text_add(out,L"%ls影响预检。%ls [ID:%d] 的直接依赖 %d 个，当前状态 %d。\n",action,name,feature.id,count,state);
    if(count){
        part_text_add(out,L"下游特征依赖目标参照，实际操作前需核对这些参照。\n");
        error=qa_dependency_walk(view->model,feature,0,(int)as_arg(cmd,4),out);
        if(error)return error;
    }
    part_text_add(out,L"范围为当前零件原生树。外部装配、关系式及改后几何需单独复核。");
    return PRO_TK_NO_ERROR;
}
