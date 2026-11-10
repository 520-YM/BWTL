static ProError vt_gland(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int sheet,PartText *out){
    double d=as_arg(cmd,10);
    PanelProfile opening={wcscmp(cmd->arg[9],L"ROUND")?PN_FLAT:PN_CIRCLE,as_arg(cmd,6),as_arg(cmd,7),d,d,as_arg(cmd,11),0};
    return vt_grip_cut(tx,cmd,frame,opening,sheet,out);
}
