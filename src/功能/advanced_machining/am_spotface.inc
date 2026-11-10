static ProError am_spotface(FdTxn *tx,const PartCommand *cmd,AmHole *geometry){
    return am_axis(tx,cmd,geometry,1);
}
