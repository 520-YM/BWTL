static ProError md_boss(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch,bore;ProMatrix frame;ProVector direction;double radius=as_arg(cmd,5)/2,height=as_arg(cmd,6),hole=as_arg(cmd,7)/2,depth=as_arg(cmd,8);
    ProError e;if((e=md_frame(tx,cmd->arg[3],cmd->arg[4],2,&f)))return e;
    if((e=md_profile(tx,cmd->arg[4],&f,0,0,L"BASE",NULL,1,&radius,cmd->arg[3],sketch,frame))||
       (e=md_extrude(tx,tx->name,sketch,frame,f.csys.z_vector,height,0)))return e;
    if(hole>0){
        swprintf_s(bore,PRO_NAME_SIZE,L"%.23ls_BORE",tx->name);for(int i=0;i<3;i++)direction[i]=-f.csys.z_vector[i];
        if((e=md_profile(tx,cmd->arg[4],&f,0,height,L"HOLE",NULL,1,&hole,NULL,sketch,frame))||
           (e=md_extrude(tx,bore,sketch,frame,direction,depth,1)))return e;
    }
    return md_expected_volume(tx,AS_PI*(radius*radius*height-hole*hole*depth));
}
