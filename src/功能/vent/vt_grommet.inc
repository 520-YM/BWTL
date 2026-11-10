static ProError vt_grommet(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int sheet,PartText *out){
    double d=as_arg(cmd,10);
    int rect=!wcscmp(cmd->arg[9],L"RECT");
    PanelProfile opening={rect?PN_ROUNDED:PN_CIRCLE,as_arg(cmd,6),as_arg(cmd,7),d,rect?as_arg(cmd,11):d,as_arg(cmd,12),0};
    return vt_grip_cut(tx,cmd,frame,opening,sheet,out);
}
