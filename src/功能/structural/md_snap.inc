static ProError md_snap(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch;ProMatrix frame;ProError e;
    double length=as_arg(cmd,5),width=as_arg(cmd,6),thickness=as_arg(cmd,7),hook=as_arg(cmd,8),ramp=as_arg(cmd,9),embed=fmin(0.2,thickness/10);
    if((e=md_frame(tx,cmd->arg[3],cmd->arg[4],0,&f)))return e;
    const double p[][3]={{-embed,-width/2,0},{length,-width/2,0},{length,-width/2,thickness},{length-ramp,-width/2,thickness+hook},{length-ramp,-width/2,thickness},{-embed,-width/2,thickness}};
    if((e=md_profile(tx,cmd->arg[4],&f,1,-width/2,L"HOOK",p,6,NULL,NULL,sketch,frame))||
       (e=md_extrude(tx,tx->name,sketch,frame,f.csys.y_vector,width,0)))return e;
    return md_expected_volume(tx,width*(length*thickness+ramp*hook/2));
}
