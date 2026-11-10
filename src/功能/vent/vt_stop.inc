static ProError vt_stop(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,PartText *out){
    double t=as_arg(cmd,8),height=as_arg(cmd,11);
    PanelProfile step=pn_rect(as_arg(cmd,6),as_arg(cmd,7),as_arg(cmd,9),as_arg(cmd,10));
    ProName sketch;
    ProMatrix matrix;
    ProError error=mp_rect_samples(tx,frame,&step,0,t,1);
    if(error)return error;
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_SK",tx->name);
    if((error=pn_sketch(tx->model,cmd,frame,sketch,&step,1,0,matrix))||
       (error=md_extrude(tx,tx->name,sketch,matrix,frame->placement.csys.z_vector,height,0))||
       (error=md_expected_volume(tx,pn_area(&step)*height)))return error;
    double lift=height+0.5;
    PanelFrame raised=vt_raised_frame(frame,lift,tx->scale);
    for(int i=-1;i<=1;i++)for(int j=-1;j<=1;j++){
        AsPoint p=as_point(step.x+i*step.width*0.45,step.y+j*step.height*0.45);
        if((error=pn_probe(tx,&raised,p,lift-height,lift+t,0)))return error;
    }
    part_text_add(out,L"已创建矩形一体挡水台阶，宽 %.8g mm、长 %.8g mm、实测高度 %.8g mm。",
        step.width,step.height,height);
    return PRO_TK_NO_ERROR;
}
