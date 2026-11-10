static ProError ax_field_single_part(ProMdl assembly,AxComponent *target){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,0,&scan);
    if(error)return error;
    AxOccurrence *selected=NULL;
    for(int i=0;i<scan->count;i++)
        if(ax_field_path_matches(&target->path,&scan->occurrences[i]))selected=&scan->occurrences[i];
    int occurrences=0;
    if(selected)for(int i=0;i<scan->count;i++)
        if(ax_field_same_part(selected,&scan->occurrences[i]))occurrences++;
    int complete=!scan->limited&&!scan->unavailable&&!scan->suppressed;
    free(scan);
    return complete&&occurrences==1?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError ax_field_mark(ProMdl assembly,const PartCommand *cmd,PartText *out){
    AxComponent target={0};
    AxDeviceFrame frame;
    double mm;
    ProError error=ax_component_resolve(assembly,cmd->arg[2],&target);
    if(!error)error=ax_ordinary_direct(&target.feature);
    if(!error)error=ax_field_single_part(assembly,&target);
    if(error){
        part_text_add(out,L"定位标记需要总装中唯一使用的活动直接零件，且组件树可完整核对。\n");
        return error;
    }
    if((error=ax_device_frame(&target,cmd->arg[4],0,&frame))||
       (error=qa_mm_per_unit(target.model,&mm)))return error;
    AxCopyGeometry line={PRO_CURVE,2,{0}};
    for(int i=0;i<2;i++){
        memcpy(line.points[i],frame.origin,sizeof(ProVector));
        for(int axis=0;axis<3;axis++)
            for(int j=0;j<3;j++)
                line.points[i][j]+=frame.axis[axis][j]*as_arg(cmd,5+3*i+axis)/mm;
    }
    if(ad_near(line.points[0],line.points[1],0.01/mm))return PRO_TK_BAD_INPUTS;
    AxCopyTransaction tx={0};
    tx.target=target.model;
    error=ax_copy_make(&tx,cmd->arg[3],cmd->arg[4],&line,&frame,mm);
    if(!error)error=part_regenerate(assembly);
    if(!error){
        part_text_add(out,L"已在零件实例 %ls 创建定位直线 %ls，按 %ls 复核了两个端点，未保存。\n",
            cmd->arg[2],cmd->arg[3],cmd->arg[4]);
    }else if(tx.count){
        ProFeature feature;
        if(ux_named_feature(target.model,cmd->arg[3],&feature)||
           model_remove_feature((ProSolid)target.model,feature.id)||
           part_regenerate(target.model)||part_regenerate(assembly)||
           ux_named_feature(target.model,cmd->arg[3],&feature)!=PRO_TK_E_NOT_FOUND)
            ax_uncertain(out);
    }
    return error;
}
