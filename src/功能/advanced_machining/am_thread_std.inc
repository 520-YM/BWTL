static ProError am_thread_std(FdTxn *tx,const PartCommand *cmd,McProfiledHole *hole){
    if(wcscmp(cmd->arg[8],L"UNC")&&wcscmp(cmd->arg[8],L"UNF"))return PRO_TK_BAD_INPUTS;
    return am_thread(tx,cmd,hole);
}
