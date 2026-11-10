static ProError ax_move(ProMdl assembly,const PartCommand *cmd,double scale,PartText *out){
    AxComponent component={0};
    ProMatrix before,initial,wanted,actual;
    ProError error;
    if((error=ax_component_resolve(assembly,cmd->arg[2],&component))||
       (error=ax_ordinary_direct(&component.feature))||
       (error=ax_leaf_without_children(&component.feature))||
       (error=ax_packaged(&component.feature))||
       (error=ax_actual_pose(assembly,&component.feature,before))||
       (error=ProAsmcompPositionGet(&component.feature,initial)))return error;
    memcpy(wanted,before,sizeof(wanted));
    for(int i=0;i<3;i++){
        if(fabs(before[3][i]*scale-wcstod(cmd->arg[3+i],NULL))>1e-5)return PRO_TK_BAD_CONTEXT;
        wanted[3][i]=wcstod(cmd->arg[6+i],NULL)/scale;
    }
    if(ax_same_pose(before,wanted,scale))return PRO_TK_NO_CHANGE;
    error=ax_set_position(assembly,&component.feature,wanted);
    if(!error)error=ax_actual_pose(assembly,&component.feature,actual);
    if(!error&&!ax_same_pose(wanted,actual,scale))error=PRO_TK_GENERAL_ERROR;
    if(error){
        if(ax_set_position(assembly,&component.feature,initial)||ax_actual_pose(assembly,&component.feature,actual)||
           !ax_same_pose(before,actual,scale))ax_uncertain(out);
    }else part_text_add(out,L"%ls 已移到 (%.9g, %.9g, %.9g) mm。原旋转姿态保持，已核对实际位置。\n",
        cmd->arg[2],actual[3][0]*scale,actual[3][1]*scale,actual[3][2]*scale);
    return error;
}
