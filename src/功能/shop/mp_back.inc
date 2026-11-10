static ProError mp_recess(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,
    double diameter,double depth,double remaining,int tool,PartText *out){
    ShopMouth mouth={0};
    ProError error=mp_mouth(tx,cmd->arg[3],cmd->arg[6],&mouth);
    if(error)return error;
    AsPoint center=fx_local_xy(&setup->frame,mouth.frame.placement.csys.origin,tx->scale);
    double radius=diameter/2;
    if(radius-mouth.radius<=0.1)return PRO_TK_BAD_INPUTS;
    if((error=pn_probe(tx,&setup->frame,center,0,0,0)))return error;
    for(int i=0;i<3;i++){
        double sample=mouth.radius+(radius-mouth.radius)*(0.15+0.375*i);
        if((error=mp_circle_probes(tx,&setup->frame,center,sample,0,depth+remaining,1)))return error;
    }
    PanelProfile profile=mp_circle(center,diameter);
    if((error=mp_blind(tx,setup,L"SK",&profile,1,depth))||
       (error=md_expected_volume(tx,-AS_PI*(radius*radius-mouth.radius*mouth.radius)*depth))||
       (error=mp_circle_probes(tx,&setup->frame,center,(radius+mouth.radius)/2,depth,depth+remaining,0))||
       (error=pn_probe(tx,&setup->frame,center,0,0,0)))return error;
    part_text_add(out,L"已创建%ls，直径 %.8g mm，深 %.8g mm。实测环形底部留料至少 %.8g mm，原孔仍贯穿。",
        tool?L"螺钉头工具让位":L"孔背面沉台",diameter,depth,remaining);
    return PRO_TK_NO_ERROR;
}

static ProError mp_back(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    return mp_recess(tx,cmd,setup,as_arg(cmd,7),as_arg(cmd,8),as_arg(cmd,9),0,out);
}
