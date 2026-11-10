static ProError mp_dog(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    double x=as_arg(cmd,6),y=as_arg(cmd,7),w=as_arg(cmd,8),h=as_arg(cmd,9);
    double depth=as_arg(cmd,10),diameter=as_arg(cmd,11),r=diameter/2,minimum=depth+0.1;
    PanelProfile pocket=pn_rect(x,y,w,h),profiles[4];
    ProError error=mp_rect_samples(tx,&setup->frame,&pocket,depth,minimum,0);
    if(error)return error;
    int index=0;
    for(int sx=-1;sx<=1;sx+=2)for(int sy=-1;sy<=1;sy+=2){
        AsPoint center=as_point(x+sx*w/2,y+sy*h/2);
        profiles[index++]=mp_circle(center,diameter);
        for(int i=-1;i<=1;i+=2)for(int j=-1;j<=1;j+=2){
            AsPoint p=as_point(center.x+sx*i*r*0.45,center.y+sy*j*r*0.45);
            int existing=i<0&&j<0;
            if((error=mp_probe_min(tx,&setup->frame,p,existing?depth:0,minimum,!existing)))return error;
        }
    }
    if((error=mp_blind(tx,setup,L"SK",profiles,4,depth))||
       (error=md_expected_volume(tx,-3*AS_PI*r*r*depth)))return error;
    for(int i=0;i<4;i++){
        const PanelProfile *p=&profiles[i];
        if((error=mp_circle_probes(tx,&setup->frame,as_point(p->x,p->y),r*0.95,depth,minimum,0)))return error;
        AsPoint outside=as_point(p->x+(p->x<x?-r:r),p->y+(p->y<y?-r:r));
        if((error=mp_probe_min(tx,&setup->frame,outside,0,minimum,0)))return error;
    }
    part_text_add(out,L"已在现有矩形型腔的四个内角创建直径 %.8g mm 的过切避让，实测深度 %.8g mm。",diameter,depth);
    return PRO_TK_NO_ERROR;
}
