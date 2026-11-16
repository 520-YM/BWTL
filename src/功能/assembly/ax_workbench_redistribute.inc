static ProError aw_distribute(ProMdl assembly,const PartCommand *cmd,int kind,double mm,PartText *out){
    AxDeviceFrame frame;
    ProError error=aw_frame_world(assembly,L"ROOT",cmd->arg[2],&frame);
    if(error)return error;
    const int axis=kind==AW_BEAM_LAYOUT?0:2;
    const double low=as_arg(cmd,3)+as_arg(cmd,5),high=as_arg(cmd,4)-as_arg(cmd,5);
    if(high<=low)return PRO_TK_BAD_INPUTS;
    const double step=as_arg(cmd,6);
    if(kind==AW_BEAM_LAYOUT&&(high-low)/step>AX_BATCH_LIMIT-1)return PRO_TK_INCOMPLETE;
    int needed=kind==AW_BEAM_LAYOUT?(int)ceil((high-low)/step)+1:(int)step;
    if(needed<2||needed>AX_BATCH_LIMIT)return PRO_TK_BAD_INPUTS;
    const double pitch=(high-low)/(needed-1);
    const int existing=cmd->count-8;
    ProMatrix before[AX_BATCH_LIMIT],wanted[AX_BATCH_LIMIT];
    AxComponent items[AX_BATCH_LIMIT]={0};
    double position[AX_BATCH_LIMIT];
    int order[AX_BATCH_LIMIT];
    for(int i=0;i<existing;i++){
        if((error=ax_hw_occurrence(assembly,cmd->arg[8+i],mm,&items[i]))||
           (error=ax_ordinary_direct(&items[i].feature))||
           (error=ax_leaf_without_children(&items[i].feature))||
           (error=ax_actual_pose(assembly,&items[i].feature,before[i])))return error;
        if(i&&items[i].model!=items[0].model){
            part_text_add(out,L"当前布局要求同一横梁或托盘型号。请分开处理不同型号。\n");return PRO_TK_BAD_CONTEXT;
        }
        position[i]=ax_project(before[i][3],&frame,axis)*mm;
        int at=i;
        while(at&&position[order[at-1]]>position[i]){order[at]=order[at-1];at--;}
        order[at]=i;
    }
    const int seed=order[0];
    ProOutline bounds;
    if((error=ax_device_world_bounds(&items[seed],&frame,bounds)))return error;
    const double required=(bounds[1][axis]-bounds[0][axis])*mm;
    if(pitch<required+0.001){
        part_text_add(out,L"计算节距 %.8g mm，小于所选组件沿布置轴的实际外廓 %.8g mm。\n",pitch,required);
        return PRO_TK_BAD_INPUTS;
    }
    if(low+mm*bounds[0][axis]-position[seed]<as_arg(cmd,3)-0.001||
       high+mm*bounds[1][axis]-position[seed]>as_arg(cmd,4)+0.001){
        part_text_add(out,L"给定边距不足以包住首末实例的实际外廓。\n");return PRO_TK_BAD_INPUTS;
    }
    if(existing>needed){
        part_text_add(out,L"现有 %d 个，目标 %d 个。须先明确选出保留实例，本次未定位或删件。\n",existing,needed);
        return PRO_TK_BAD_CONTEXT;
    }
    for(int i=0;i<needed;i++){
        int old=i<existing?order[i]:seed;
        memcpy(wanted[i],before[old],sizeof(ProMatrix));
        double anchor=low+i*pitch;
        for(int j=0;j<3;j++)wanted[i][3][j]+=(anchor-position[old])*frame.axis[axis][j]/mm;
        part_text_add(out,L"第 %d 个目标站位 %lc %.8g mm%ls。\n",i+1,L'X'+axis,anchor,i<existing?L"（重新定位）":L"（新增同型号）");
    }
    part_text_add(out,L"首末为 %.8g / %.8g mm，%d 个站位，节距 %.8g mm。\n",low,high,needed,pitch);
    if(!wcscmp(cmd->arg[7],L"PREVIEW"))return PRO_TK_NO_ERROR;

    for(int i=0;i<existing;i++)if((error=ax_packaged(&items[i].feature)))return error;
    int changed=0;
    AxBatchItem additions[AX_BATCH_LIMIT]={0};
    for(int i=0;!error&&i<existing;i++){
        int old=order[i];
        changed++;
        error=ax_set_position(assembly,&items[old].feature,wanted[i]);
        ProMatrix actual;
        if(!error)error=ax_actual_pose(assembly,&items[old].feature,actual);
        if(!error&&!ax_same_pose(wanted[i],actual,mm))error=PRO_TK_INCOMPLETE;
    }
    for(int i=existing;i<needed;i++){
        additions[i-existing].model=items[seed].model;
        memcpy(additions[i-existing].pose,wanted[i],sizeof(ProMatrix));
    }
    if(!error&&needed>existing)error=ax_batch_create(assembly,additions,needed-existing,mm,0,out);
    if(!error)error=part_health(assembly);
    if(!error)part_text_add(out,L"%ls布局完成：调整 %d 个现有位置，新增 %d 个同型号实例，原生再生通过，未保存。\n",
        kind==AW_BEAM_LAYOUT?L"横梁":L"托盘",changed,needed-existing);
    else{
        for(int i=needed-existing-1;i>=0;i--)
            if(additions[i].feature.owner){
                /* ax_batch_create may already have removed a failed instance. */
                ProAsmcomp test;
                if(!ax_component_feature(assembly,additions[i].feature.id,&test))ax_discard_occurrence(assembly,&test,out);
            }
        int bad=0;
        for(int i=changed-1;i>=0;i--){
            int old=order[i];
            ProMatrix actual;
            if(ax_set_position(assembly,&items[old].feature,before[old])||
               ax_actual_pose(assembly,&items[old].feature,actual)||!ax_same_pose(before[old],actual,mm))bad=1;
        }
        if(bad||part_health(assembly))ax_uncertain(out);
        else if(!ax_recovery_uncertain)part_text_add(out,L"已核对本次组件位置回滚。\n");
    }
    return error;
}
