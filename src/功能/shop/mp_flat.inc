static ProError mp_flat(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    ShopMouth mouth={0};
    ProError error=mp_mouth(tx,cmd->arg[3],cmd->arg[6],&mouth);
    if(error)return error;
    ShopSetup rotated=*setup;
    ProCsysdata *csys=&rotated.frame.placement.csys;
    const ProCsysdata *base=&setup->frame.placement.csys;
    double width=as_arg(cmd,7),radial=as_arg(cmd,8),angle=as_arg(cmd,9)*AS_PI/180;
    if(width>=2*mouth.radius-0.1)return PRO_TK_BAD_INPUTS;
    memcpy(csys->origin,mouth.frame.placement.csys.origin,sizeof(ProVector));
    for(int i=0;i<3;i++){
        csys->x_vector[i]=cos(angle)*base->x_vector[i]+sin(angle)*base->y_vector[i];
        csys->y_vector[i]=-sin(angle)*base->x_vector[i]+cos(angle)*base->y_vector[i];
    }
    double thickness;
    if((error=pn_probe(tx,&rotated.frame,as_point(0,0),0,0,0))||
       (error=mp_read_stock(tx,&rotated.frame,as_point(mouth.radius+radial*0.5,0),0,&thickness,1)))return error;
    for(int i=-1;i<=1;i++)if((error=pn_probe(tx,&rotated.frame,as_point(mouth.radius+radial*0.9,i*width*0.4),0,thickness,1)))return error;
    PanelProfile profile={PN_NOTCH,0,0,2*mouth.radius,width,radial,0};
    if((error=mp_blind(tx,&rotated,L"SK",&profile,1,thickness))||
       (error=md_expected_volume(tx,-(pn_area(&profile)-AS_PI*mouth.radius*mouth.radius)*thickness)))return error;
    for(int i=-1;i<=1;i++)if((error=pn_probe(tx,&rotated.frame,as_point(mouth.radius+radial*0.98,i*width*0.4),0,0,0)))return error;
    if((error=pn_probe(tx,&rotated.frame,as_point(mouth.radius+radial+0.05,0),0,thickness,0)))return error;
    part_text_add(out,L"已在原圆孔内创建定向直壁止转缺口，缺口宽 %.8g mm，径向加深 %.8g mm，方位 %.8g°。",
        width,radial,as_arg(cmd,9));
    return PRO_TK_NO_ERROR;
}
