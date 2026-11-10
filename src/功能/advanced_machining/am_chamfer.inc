static ProError am_chamfer(FdTxn *tx,const PartCommand *cmd){
    return am_edge(tx,cmd,AM_CHAMFER);
}
