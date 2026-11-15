static ProError smy_spot_welds(ProMdl model,const PartCommand *cmd,PartText *out){
    FdTxn view={0};
    PanelFrame frame={0};
    double mm,t;
    ProError error=fab_thickness(model,&mm,&t);
    if(error)return error;
    view.model=model;view.scale=1/mm;
    error=pn_frame(&view,cmd,&frame);
    if(error)return error;
    double x=as_arg(cmd,6),y=as_arg(cmd,7),dx=as_arg(cmd,8),dy=as_arg(cmd,9);
    double length=hypot(dx,dy),pitch=as_arg(cmd,10),dia=as_arg(cmd,11),margin=as_arg(cmd,12);
    if(length<0.001||pitch<dia+margin)return PRO_TK_BAD_INPUTS;
    int count=(int)floor(length/pitch+1e-8)+1;
    if(count<2||count>512)return PRO_TK_OUT_OF_RANGE;
    double start=(length-(count-1)*pitch)/2;
    for(int i=0;i<count;i++){
        double at=(start+i*pitch)/length;
        PanelProfile point={PN_CIRCLE,x+dx*at,y+dy*at,dia,dia};
        error=pn_profile_probes(&view,&frame,&point,t,0);
        if(!error)error=smx_outline_clearance(&view,&frame,&point,margin);
        if(error){
            part_text_add(out,L"焊点 %d 的电极区超出有效板面、接近折弯或孔边，未交付整组。\n",i+1);
            return error;
        }
    }
    DeliveryOutput work={0};
    FILE *file=NULL;
    error=dv_stage_begin(cmd->arg[2],&work);
    if(!error)error=fab_csv_open(work.stage,L"spot-weld-layout.csv",&file);
    if(!error&&fwprintf(file,L"焊点,板面,X_mm,Y_mm,Z_mm,电极直径_mm,板厚_mm\n")<0)error=PRO_TK_CANT_WRITE;
    for(int i=0;!error&&i<count;i++){
        double at=(start+i*pitch)/length;
        ProVector point;
        am_world(&frame.placement,(x+dx*at)/mm,(y+dy*at)/mm,0,point);
        if(fwprintf(file,L"%d,%ls,%.10g,%.10g,%.10g,%.10g,%.10g\n",i+1,cmd->arg[3],point[0]*mm,point[1]*mm,point[2]*mm,dia,t)<0)
            error=PRO_TK_CANT_WRITE;
    }
    error=dv_stage_finish(&work,fab_close(file,error),out);
    if(!error)part_text_add(out,L"已排布 %d 个点焊位置，节距 %.8g mm。已核对所选板面的电极落点、板厚及净边；贴合与另一侧电极空间由工艺复核。%ls。\n",
        count,pitch,work.target);
    return error;
}

static ProError smy_plug_slot(ProMdl model,const PartCommand *cmd,PartText *out){
    double diameter=as_arg(cmd,8),length=as_arg(cmd,11),width=as_arg(cmd,12);
    if(length<=width)return PRO_TK_BAD_INPUTS;
    PanelProfile profiles[]={
        {PN_CIRCLE,as_arg(cmd,6),as_arg(cmd,7),diameter,diameter},
        {PN_CAPSULE,as_arg(cmd,9),as_arg(cmd,10),length,width}
    };
    ProError error=smx_openings_apply(model,cmd,profiles,_countof(profiles),as_arg(cmd,13),0,out);
    if(!error)part_text_add(out,L"已创建塞焊孔 Ø%.8g mm 与定位槽 %.8g × %.8g mm，两处贯穿及孔槽间料桥已复核。未保存。\n",diameter,length,width);
    return error;
}

