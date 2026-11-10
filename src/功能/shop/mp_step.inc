#ifdef BWTL_SHOP_FAULT_TEST
static int mp_test_fail_after_first_cut,mp_test_fault_hit;
#endif

static ProError mp_steps(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,int round,PartText *out){
    int count=(int)as_arg(cmd,8),stride=round?2:3;
    PanelProfile profiles[5];
    ProName sketches[5],features[5];
    ProMatrix frames[5];
    double depths[5],total=0,previous=0;
    AsPoint center=as_point(as_arg(cmd,6),as_arg(cmd,7));
    for(int i=0;i<count;i++){
        int at=9+stride*i;
        profiles[i]=round?mp_circle(center,as_arg(cmd,at)):pn_rect(center.x,center.y,as_arg(cmd,at),as_arg(cmd,at+1));
        depths[i]=as_arg(cmd,at+stride-1);
        total+=pn_area(&profiles[i])*(depths[i]-previous);
        previous=depths[i];
    }
    double minimum=depths[count-1]+0.1;
    ProError error=mp_probe_min(tx,&setup->frame,center,0,minimum,1);
    if(error)return error;
    if(round)error=mp_circle_probes(tx,&setup->frame,center,profiles[0].width*0.48,0,minimum,1);
    else for(int x=-1;x<=1&&!error;x++)for(int y=-1;y<=1&&!error;y++){
        AsPoint p={center.x+x*profiles[0].width*0.48,center.y+y*profiles[0].height*0.48};
        error=mp_probe_min(tx,&setup->frame,p,0,minimum,1);
    }
    if(error)return error;
    for(int i=0;i<count;i++){
        ProName suffix;
        swprintf_s(suffix,PRO_NAME_SIZE,L"SK%d",i+1);
        if((error=mp_prepare_profile(tx,setup,suffix,&profiles[i],1,sketches[i],frames[i])))return error;
        if(i==0)wcscpy_s(features[i],PRO_NAME_SIZE,tx->name);
        else swprintf_s(features[i],PRO_NAME_SIZE,L"%ls_D%d",tx->name,i+1);
    }
    for(int i=0;i<count;i++){
        if((error=pn_cut(tx,&setup->frame,features[i],sketches[i],frames[i],depths[i],0)))return error;
#ifdef BWTL_SHOP_FAULT_TEST
        if(!i&&mp_test_fail_after_first_cut){mp_test_fault_hit=1;return PRO_TK_GENERAL_ERROR;}
#endif
    }
    if((error=md_expected_volume(tx,-total)))return error;
    for(int i=0;i<count;i++){
        double w=(profiles[i].width+(i+1<count?profiles[i+1].width:0))/4;
        double h=(profiles[i].height+(i+1<count?profiles[i+1].height:0))/4;
        if(round)error=mp_circle_probes(tx,&setup->frame,center,w,depths[i],minimum,0);
        else {
            AsPoint points[]={{center.x+w,center.y},{center.x-w,center.y},{center.x,center.y+h},{center.x,center.y-h}};
            for(int j=0;j<_countof(points)&&!error;j++)error=mp_probe_min(tx,&setup->frame,points[j],depths[i],minimum,0);
        }
        if(error)return error;
    }
    part_text_add(out,round?L"已创建 %d 级同轴盲孔，逐级核对台阶和孔底，最深 %.8g mm。":
        L"已创建 %d 层同心矩形型腔，逐层核对底面深度，最深 %.8g mm。",count,depths[count-1]);
    return PRO_TK_NO_ERROR;
}

static ProError mp_step(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    return mp_steps(tx,cmd,setup,1,out);
}
