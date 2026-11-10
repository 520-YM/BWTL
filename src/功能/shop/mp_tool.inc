static ProError mp_tool(FdTxn *tx,const PartCommand *cmd,const ShopSetup *setup,PartText *out){
    return mp_recess(tx,cmd,setup,as_arg(cmd,7)+2*as_arg(cmd,8),as_arg(cmd,9),as_arg(cmd,10),1,out);
}
