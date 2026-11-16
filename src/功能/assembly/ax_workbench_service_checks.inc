static ProError aw_voltage_clearance(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent a={0},b={0};
    ProSelection selections[2]={0};
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&a);
    if(!error)error=ax_hw_occurrence(assembly,cmd->arg[3],mm,&b);
    if(!error)error=ax_part_selection(&a,&selections[0]);
    if(!error)error=ax_part_selection(&b,&selections[1]);
    double gap=0;
    int collision=0;
    if(!error)error=ax_motion_sample_fit(selections[0],selections[1],mm,&gap,&collision);
    for(int i=0;i<2;i++)if(selections[i])ProSelectionFree(&selections[i]);
    if(error)return error;
    const double required=as_arg(cmd,4);
    int pass=!collision&&qa_at_least(gap,required);
    part_text_add(out,L"高压组件 %ls、低压组件 %ls，原生最小空气间隙 %.8g mm（要求 ≥ %.8g），%ls。\n",
        cmd->arg[2],cmd->arg[3],gap,required,collision?L"有实体干涉":pass?L"符合":L"间隙不足");
    if(cmd->count==5)return PRO_TK_NO_ERROR;

    AxQualityFace divider={0};
    if((error=ax_q_face(assembly,cmd->arg[5],cmd->arg[6],mm,&divider)))return error;
    AxDeviceFrame frame={0};
    memcpy(frame.origin,divider.plane.point,sizeof(ProVector));
    ax_hw_basis(divider.plane.normal,frame.axis);
    memcpy(frame.axis[2],divider.plane.normal,sizeof(ProVector));
    ProOutline first,second;
    if((error=ax_device_world_bounds(&a,&frame,first))||(error=ax_device_world_bounds(&b,&frame,second)))return error;
    const double epsilon=0.001/mm;
    int separated=(first[1][2]<-epsilon&&second[0][2]>epsilon)||(second[1][2]<-epsilon&&first[0][2]>epsilon);
    double low[3],high[3];
    if((error=aw_project_face(&divider,&frame,mm,low,high)))return error;
    int overlap=1;
    for(int i=0;i<2;i++)
        if(low[i]>fmin(first[0][i],second[0][i])*mm+0.001||
           high[i]<fmax(first[1][i],second[1][i])*mm-0.001)overlap=0;
    part_text_add(out,L"指定隔板两侧分区：%ls。几何投影覆盖：%ls。隔板连续性和绝缘材料需按项目规范核对。\n",
        separated?L"两个组件分别位于两侧":L"没有完整分隔",
        overlap?L"外廓覆盖":L"不足");
    return PRO_TK_NO_ERROR;
}

static ProError aw_seal_loop(ProMdl assembly,const PartCommand *cmd,double mm,PartText *out){
    AxComponent carrier={0};
    ProError error=ax_hw_occurrence(assembly,cmd->arg[2],mm,&carrier);
    if(error)return error;
    const int n=cmd->count-10;
    ProVector endpoints[AX_BATCH_LIMIT][2];
    for(int i=0;i<n;i++){
        ProEdge edge;
        ProEnttype type;
        if((error=ax_device_edge(carrier.model,cmd->arg[10+i],&edge))||(error=ProEdgeTypeGet(edge,&type)))return error;
        if(type!=PRO_ENT_LINE)return PRO_TK_INVALID_TYPE;
        for(int j=0;j<2;j++)if((error=ProEdgeXyzdataEval(edge,j,endpoints[i][j],NULL,NULL,NULL)))return error;
    }
    const double limit=as_arg(cmd,9),g0=aw_distance3(endpoints[0][0],endpoints[1][0]),g1=aw_distance3(endpoints[0][0],endpoints[1][1]);
    const double g2=aw_distance3(endpoints[0][1],endpoints[1][0]),g3=aw_distance3(endpoints[0][1],endpoints[1][1]);
    const int first_start=fmin(g0,g1)<fmin(g2,g3)?1:0;
    ProVector last_end;
    memcpy(last_end,endpoints[0][1-first_start],sizeof(last_end));
    int missing=0;
    double worst=0;
    for(int i=1;i<=n;i++){
        const int next=i%n;
        const double a=aw_distance3(last_end,endpoints[next][0])*mm,b=aw_distance3(last_end,endpoints[next][1])*mm;
        const int start=i==n?first_start:(a<=b?0:1);
        const double distance=start?b:a;
        worst=fmax(worst,distance);
        int pass=ax_q_within(distance,limit);
        missing+=!pass;
        part_text_add(out,L"密封第 %d 段末端 → 第 %d 段起点，实际断点 %.8g mm，%ls。\n",
            i,next+1,distance,pass?L"连续":L"需核对或搭接");
        memcpy(last_end,endpoints[next][1-start],sizeof(last_end));
    }
    part_text_add(out,L"%d 段闭环，最大断点 %.8g mm，超限接头 %d 个（最大允许 %.8g mm）。\n",n,worst,missing,limit);
    PartCommand compression=*cmd;
    compression.count--;
    for(int i=9;i<compression.count;i++)wcscpy_s(compression.arg[i],PART_TOKEN_MAX,cmd->arg[i+1]);
    return ax_device_seal(assembly,&compression,mm,out);
}
