static ProError vt_seal_witness(FdTxn *tx,const PanelFrame *frame,AsPoint center,double w,double h,double channel,
    double front,double back,int before){
    double x=(w-channel)/2,y=(h-channel)/2;
    const AsPoint samples[]={{x,y},{-x,y},{-x,-y},{x,-y},{x,0},{-x,0},{0,y},{0,-y}};
    for(int i=0;i<_countof(samples);i++){
        ProError error=pn_probe(tx,frame,as_plus(center,samples[i]),front,back,before);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError vt_seal(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,PartText *out){
    AsPoint center=as_point(as_arg(cmd,6),as_arg(cmd,7));
    double t=as_arg(cmd,8),w=as_arg(cmd,9),h=as_arg(cmd,10),channel=as_arg(cmd,11),depth=as_arg(cmd,12);
    PanelProfile profiles[]={pn_rect(center.x,center.y,w,h),pn_rect(center.x,center.y,w-2*channel,h-2*channel)};
    ShopSetup setup={*cmd,*frame};
    ProError error=vt_seal_witness(tx,frame,center,w,h,channel,0,t,1);
    if(error)return error;
    if((error=mp_blind(tx,&setup,L"SK",profiles,2,depth))||
       (error=md_expected_volume(tx,-(pn_area(&profiles[0])-pn_area(&profiles[1]))*depth))||
       (error=vt_seal_witness(tx,frame,center,w,h,channel,depth,t,0)))return error;
    const AsPoint keep[]={{0,0},{(w-2*channel)/2-0.05,0},{0,-(h-2*channel)/2+0.05}};
    for(int i=0;i<_countof(keep);i++)
        if((error=pn_probe(tx,frame,as_plus(center,keep[i]),0,t,0)))return error;
    part_text_add(out,L"已创建闭合矩形密封槽，槽宽 %.8g mm、槽深 %.8g mm，八处实测槽底留料 %.8g mm。",
        channel,depth,t-depth);
    return PRO_TK_NO_ERROR;
}
