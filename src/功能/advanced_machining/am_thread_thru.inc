static ProError am_thread_thru(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole){
    if(wcscmp(cmd->arg[8],L"ISO")||wcscmp(cmd->arg[10],L"THROUGH"))return PRO_TK_BAD_INPUTS;
    return am_thread(tx,cmd,hole);
}
