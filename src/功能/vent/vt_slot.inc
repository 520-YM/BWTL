static ProError vt_slot(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int sheet,PartText *out){
    return vt_pattern(tx,cmd,frame,sheet,PN_CAPSULE,out);
}
