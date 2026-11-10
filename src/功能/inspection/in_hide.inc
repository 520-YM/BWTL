static ProError in_hide(const Inspection *inspect,const PartCommand *cmd,PartText *out){
    int bore=!wcscmp(cmd->arg[3],L"BORE");
    RevisionHole hole;
    ProError error=in_native_hole(inspect,cmd->arg[2],bore?RV_BORE:RV_SINK,&hole);
    if(error)return error;
    double outer=hole.head*inspect->mm,pilot=hole.diameter*inspect->mm,head=as_arg(cmd,4),height=as_arg(cmd,5);
    double margin=bore?hole.shape*inspect->mm-height:(outer-head)/(2*tan(hole.shape*AS_PI/360));
    int shape=bore||(fabs(hole.shape-as_arg(cmd,6))<=0.001&&
        head/2-height*tan(hole.shape*AS_PI/360)>=-0.0002);
    int fit=shape&&head>pilot+0.0002&&qa_at_least(outer,head);
    part_text_add(out,L"沉孔藏头%ls。沉孔外径 %.8g mm，底孔直径 %.8g mm，头部直径余量 %.8g mm。",
        fit&&qa_at_least(margin,as_arg(cmd,7))?L"符合":L"不符合",outer,pilot,outer-head);
    if(fit)part_text_add(out,L"头顶低于入口面 %.8g mm，要求至少 %.8g mm。",margin,as_arg(cmd,7));
    else if(!shape)part_text_add(out,L"锥头角度或锥头高度与实际孔型不匹配。");
    return PRO_TK_NO_ERROR;
}
