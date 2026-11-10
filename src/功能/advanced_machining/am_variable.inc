static ProError am_variable(FdTxn *tx,const PartCommand *cmd){
    return am_edge(tx,cmd,AM_VARIABLE);
}
