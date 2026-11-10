static ProError md_seal_face(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch;ProMatrix frame;ProVector direction;ProError e;
    double diameter=as_arg(cmd,5),section=as_arg(cmd,6),compression=as_arg(cmd,7),width=as_arg(cmd,8),depth=section*(1-compression/100);
    double radii[]={(diameter-width)/2,(diameter+width)/2};
    if((e=md_frame(tx,cmd->arg[3],cmd->arg[4],2,&f)))return e;
    for(int i=0;i<3;i++)direction[i]=-f.csys.z_vector[i];
    if((e=md_profile(tx,cmd->arg[4],&f,0,0,L"SEAL",NULL,2,radii,cmd->arg[3],sketch,frame))||
       (e=md_extrude(tx,tx->name,sketch,frame,direction,depth,1)))return e;
    return md_expected_volume(tx,-AS_PI*diameter*width*depth);
}
