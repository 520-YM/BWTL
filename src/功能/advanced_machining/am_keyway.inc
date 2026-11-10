static ProError am_keyway(FdTxn *tx,const PartCommand *cmd){
    AmFrame f={0};ProName sketch;ProMdlName model;ProMatrix frame;ProError e;
    double start=as_arg(cmd,6)*tx->scale,width=as_arg(cmd,7)*tx->scale,length=as_arg(cmd,8)*tx->scale,depth=as_arg(cmd,9)*tx->scale;
    double y,travel,dot;
    if((e=am_frame(tx,cmd,&f)))return e;
    if(start<=f.zmin||start+length>=f.zmax||width>=1.8*f.radius||depth>=f.radius)return PRO_TK_BAD_INPUTS;
    y=f.internal?f.radius-width:f.radius-depth;
    travel=f.internal?width+depth:depth+width;
    if((e=am_profile(tx,cmd,&f,0,y,start,width,length,0,0,sketch,frame))||(e=ProMdlMdlnameGet(tx->model,model)))return e;
    dot=ad_dot(frame[2],f.csys.y_vector);
    e=am_sub(tx->model,L"CREO_OP SOLID_EXTRUDE %ls %ls %ls CUT BLIND %.15g %ls",model,tx->name,sketch,travel/tx->scale,dot<0?L"POS":L"NEG");
    return e?e:am_key_floor(tx,&f,f.internal?f.radius+depth:f.radius-depth,width,length);
}
