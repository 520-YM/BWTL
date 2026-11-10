#ifdef BWTL_VENT_FAULT_TEST
static int vt_test_fail_after_openings,vt_test_fault_hit;
#endif

static PanelFrame vt_raised_frame(const PanelFrame *frame,double offset,double scale){
    PanelFrame raised=*frame;
    cm_point(frame->placement.csys.origin,frame->placement.csys.z_vector,offset*scale,raised.placement.csys.origin);
    return raised;
}

static ProError vt_louver(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,PartText *out){
    PanelProfile openings[4];
    ProName blades[4],sketches[4];
    ProMatrix matrices[4];
    double x=as_arg(cmd,6),y=as_arg(cmd,7),t=as_arg(cmd,8),pitch=as_arg(cmd,10),w=as_arg(cmd,11),length=as_arg(cmd,12);
    double slope=tan(as_arg(cmd,13)*AS_PI/180),blade=as_arg(cmd,14),anchor=as_arg(cmd,15);
    double reach=w*0.75,rise=reach*slope,embed=fmin(t/3,blade/2);
    double left[4],lift=rise+blade+0.5;
    int count=(int)as_arg(cmd,9);
    if(count>1&&pitch-w-anchor<0.1)return PRO_TK_BAD_INPUTS;
    PanelFrame raised=vt_raised_frame(frame,lift,tx->scale);
    ProError error;
    for(int i=0;i<count;i++){
        double center=x+(i-(count-1)*0.5)*pitch;
        left[i]=center-w/2;
        openings[i]=pn_rect(center,y,w,length);
        if((error=pn_profile_probes(tx,frame,&openings[i],t,0)))return error;
        for(int j=-1;j<=1;j++){
            AsPoint root=as_point(left[i]-anchor/2,y+j*length*0.45);
            if((error=pn_probe(tx,frame,root,0,t,1)))return error;
        }
    }
    for(int i=0;i<count;i++){
        double a=left[i],b=a+reach,y0=y-length/2;
        const double section[][3]={{a-anchor,y0,-embed},{a-anchor,y0,blade},{a,y0,blade},
            {b,y0,rise+blade},{b,y0,rise},{a,y0,0}};
        ProName suffix;
        swprintf_s(suffix,PRO_NAME_SIZE,L"V%d",i+1);
        swprintf_s(blades[i],PRO_NAME_SIZE,L"%ls_%ls",tx->name,suffix);
        if((error=md_profile(tx,cmd->arg[4],&frame->placement,1,y0,suffix,section,_countof(section),
            NULL,NULL,sketches[i],matrices[i])))return error;
    }
    if((error=vt_through(tx,cmd,frame,openings,count,0,out)))return error;
#ifdef BWTL_VENT_FAULT_TEST
    if(vt_test_fail_after_openings){vt_test_fault_hit=1;return PRO_TK_GENERAL_ERROR;}
#endif
    for(int i=0;i<count;i++)if((error=md_extrude(tx,blades[i],sketches[i],matrices[i],
        frame->placement.csys.y_vector,length,0)))return error;
    double change=count*length*((reach+anchor)*blade-w*t);
    if((error=md_expected_volume(tx,change)))return error;
    for(int i=0;i<count;i++)for(int j=-1;j<=1;j++){
        double yy=y+j*length*0.4,a=left[i],below=rise/2;
        if((error=pn_probe(tx,&raised,as_point(a+reach/2,yy),lift-below-blade,lift-below,0))||
           (error=pn_probe(tx,&raised,as_point(a+(reach+w)/2,yy),0,0,0))||
           (error=pn_probe(tx,&raised,as_point(a-anchor/2,yy),lift-blade,lift+t,0)))return error;
    }
    part_text_add(out,L"已核对 %d 片一体斜叶及保留通风缝。斜叶角度 %.8g°，开口宽 %.8g mm，成品未覆盖缝宽 %.8g mm。",
        count,as_arg(cmd,13),w,w-reach);
    return PRO_TK_NO_ERROR;
}
