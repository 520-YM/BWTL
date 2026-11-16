static int aw_step_for(const PartCommand *cmd,int count,int id){
    for(int i=0;i<count;i++){
        int ids[AX_PATH_DEPTH];
        if(ax_path_parse(cmd->arg[3+i],ids)==1&&ids[0]==id)return i;
    }
    return -1;
}

static ProError aw_assembly_order(ProMdl assembly,const PartCommand *cmd,PartText *out){
    const int count=(int)as_arg(cmd,2);
    unsigned char edge[AX_BATCH_LIMIT][AX_BATCH_LIMIT]={{0}},finished[AX_BATCH_LIMIT]={0};
    int degree[AX_BATCH_LIMIT]={0},ignored=0;
    ProMdlName names[AX_BATCH_LIMIT];
    for(int i=0;i<count;i++){
        AxComponent component={0};
        ProError error=ax_component_resolve(assembly,cmd->arg[3+i],&component);
        if(!error)error=ProMdlMdlnameGet(component.model,names[i]);
        if(error)return error;
        int *parents=NULL,n=0;
        error=ProFeatureParentsGet(&component.feature,&parents,&n);
        if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
        if(!error)for(int j=0;j<n;j++){
            int previous=aw_step_for(cmd,count,parents[j]);
            if(previous>=0&&previous!=i)edge[previous][i]=1;
            else{
                ProAsmcomp outside;
                if(!ax_component_feature(assembly,parents[j],&outside))ignored++;
            }
        }
        if(parents)ProArrayFree((ProArray *)&parents);
        if(error)return error;
    }
    for(int i=3+count;i<cmd->count;i+=2){
        int id[AX_PATH_DEPTH],next[AX_PATH_DEPTH];
        ax_path_parse(cmd->arg[i],id);
        ax_path_parse(cmd->arg[i+1],next);
        int from=aw_step_for(cmd,count,id[0]),to=aw_step_for(cmd,count,next[0]);
        if(from<0||to<0||from==to)return PRO_TK_BAD_INPUTS;
        edge[from][to]=1;
    }
    for(int i=0;i<count;i++)for(int j=0;j<count;j++)degree[j]+=edge[i][j]!=0;
    for(int stage=0;stage<count;stage++){
        int chosen=-1;
        for(int i=0;i<count;i++)if(!finished[i]&&!degree[i]){chosen=i;break;}
        if(chosen<0){
            part_text_add(out,L"剩余站位形成循环，涉及：");
            for(int i=0;i<count;i++)if(!finished[i])part_text_add(out,L" %ls %ls",cmd->arg[3+i],names[i]);
            part_text_add(out,L"。需调整工序先后。\n");
            return PRO_TK_BAD_CONTEXT;
        }
        finished[chosen]=1;
        part_text_add(out,L"第 %d 步：%ls %ls。\n",stage+1,cmd->arg[3+chosen],names[chosen]);
        for(int i=0;i<count;i++)if(edge[chosen][i])degree[i]--;
    }
    part_text_add(out,L"按 Creo 直接组件依赖和输入的工艺顺序排序。%d 条指向本批之外组件的依赖未计入。轨迹、夹具和压铆先后等未标注约束需复核。\n",ignored);
    return PRO_TK_NO_ERROR;
}
