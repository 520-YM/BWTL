static ProError cm_end(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge tip;
    double depth,amount=as_arg(cmd,5);
    ProError error=cm_tip_for_wall(tx->model,cmd->arg[2],cmd->arg[3],&tip);
    if(!error)error=cm_tip_cut(tx,&tip,!wcscmp(cmd->arg[4],L"END"),amount,as_arg(cmd,6),&depth);
    if(!error)part_text_add(out,L"所选直边的 %ls 端已直线缩短 %.8g mm，切口深 %.8g mm，保留折弯根部。",
        cmd->arg[4],amount,depth);
    return error;
}
