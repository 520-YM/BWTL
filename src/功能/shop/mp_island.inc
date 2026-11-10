static ProError mp_rect_samples(FdTxn *tx,const PanelFrame *frame,const PanelProfile *rect,double front,double back,int original){
    for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
        AsPoint p=as_point(rect->x+x*rect->width*0.47,rect->y+y*rect->height*0.47);
        ProError error=mp_probe_min(tx,frame,p,front,back,original);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError mp_island(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    int count=(int)as_arg(cmd,11);
    double depth=as_arg(cmd,10),minimum=depth+0.1;
    PanelProfile profiles[5]={pn_rect(as_arg(cmd,6),as_arg(cmd,7),as_arg(cmd,8),as_arg(cmd,9))};
    const PanelProfile *outer=&profiles[0];
    double area=pn_area(outer);
    ProError error=mp_rect_samples(tx,&setup->frame,outer,0,minimum,1);
    if(error)return error;
    for(int i=1;i<=count;i++){
        int at=12+4*(i-1);
        PanelProfile *island=&profiles[i];
        *island=pn_rect(as_arg(cmd,at),as_arg(cmd,at+1),as_arg(cmd,at+2),as_arg(cmd,at+3));
        if(fabs(island->x-outer->x)+island->width/2+0.1>=outer->width/2||
           fabs(island->y-outer->y)+island->height/2+0.1>=outer->height/2)return PRO_TK_BAD_INPUTS;
        for(int j=1;j<i;j++)if(fabs(island->x-profiles[j].x)<(island->width+profiles[j].width)/2+0.1&&
           fabs(island->y-profiles[j].y)<(island->height+profiles[j].height)/2+0.1)return PRO_TK_BAD_INPUTS;
        if((error=mp_rect_samples(tx,&setup->frame,island,0,minimum,1)))return error;
        area-=pn_area(island);
    }
    if((error=mp_blind(tx,setup,L"SK",profiles,count+1,depth))||
       (error=md_expected_volume(tx,-area*depth)))return error;
    for(int i=1;i<=count;i++)if((error=mp_rect_samples(tx,&setup->frame,&profiles[i],0,minimum,0)))return error;
    for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2){
        AsPoint p=as_point(outer->x+x*(outer->width/2-0.025),outer->y+y*(outer->height/2-0.025));
        if((error=mp_probe_min(tx,&setup->frame,p,depth,minimum,0)))return error;
    }
    part_text_add(out,L"已创建深 %.8g mm 的矩形盲型腔，核实保留 %d 个矩形岛及实际去料体积。",depth,count);
    return PRO_TK_NO_ERROR;
}
