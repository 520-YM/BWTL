static ProError md_seal_radial(FdTxn *tx,const PartCommand *cmd){
    PartCommand ring=*cmd;double depth=as_arg(cmd,7)*(1-as_arg(cmd,8)/100);
    wcscpy_s(ring.arg[0],PART_TOKEN_MAX,L"RING_GROOVE");
    wcscpy_s(ring.arg[7],PART_TOKEN_MAX,cmd->arg[9]);swprintf_s(ring.arg[8],PART_TOKEN_MAX,L"%.15g",depth);ring.count=9;
    return am_ring(tx,&ring);
}
