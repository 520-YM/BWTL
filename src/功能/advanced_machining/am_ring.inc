static ProError am_ring(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch;ProMdlName model;ProMatrix frame;ProError e;
    double start=as_arg(cmd,6)*tx->scale,width=as_arg(cmd,7)*tx->scale,depth=as_arg(cmd,8)*tx->scale;
    double inner,outer,dot;
    if((e=am_frame(tx,cmd,&f)))return e;
    if(start<=f.zmin||start+width>=f.zmax||depth>=f.radius)return PRO_TK_BAD_INPUTS;
    inner=f.radius-depth;
    outer=f.radius+depth;
    if((e=am_profile(tx,cmd,&f,1,0,start,0,0,inner,outer,sketch,frame))||(e=ProMdlMdlnameGet(tx->model,model)))return e;
    dot=ad_dot(frame[2],f.csys.z_vector);
    e=am_sub(tx->model,L"CREO_OP SOLID_EXTRUDE %ls %ls %ls CUT BLIND %.15g %ls",model,tx->name,sketch,width/tx->scale,dot<0?L"POS":L"NEG");
    return e?e:am_ring_geometry(tx,&f,start,width,depth);
}
