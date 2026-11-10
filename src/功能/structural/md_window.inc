static ProError md_window(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch;ProMatrix frame;ProVector direction;ProError e;
    double width=as_arg(cmd,5),thickness=as_arg(cmd,6),hook=as_arg(cmd,7),gap=as_arg(cmd,8),depth=as_arg(cmd,9);
    if((e=md_frame(tx,cmd->arg[3],cmd->arg[4],0,&f)))return e;
    const double p[][3]={{0,-width/2-gap,-gap},{0,width/2+gap,-gap},{0,width/2+gap,thickness+hook+gap},{0,-width/2-gap,thickness+hook+gap}};
    for(int i=0;i<3;i++)direction[i]=-f.csys.x_vector[i];
    if((e=md_profile(tx,cmd->arg[4],&f,2,0,L"WIN",p,4,NULL,NULL,sketch,frame))||
       (e=md_extrude(tx,tx->name,sketch,frame,direction,depth,1)))return e;
    return md_expected_volume(tx,-(width+2*gap)*(thickness+hook+2*gap)*depth);
}
