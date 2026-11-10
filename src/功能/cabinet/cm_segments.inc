static ProError cm_segments(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetEdge original;
    ProError error=cm_edge_get(tx->model,cmd->arg[3],&original);
    if(error)return error;
    if(as_arg(cmd,cmd->count-1)*tx->scale>original.length+0.0001*tx->scale)return PRO_TK_BAD_INPUTS;
    for(int i=9;i<cmd->count;i+=2){
        CabinetEdge edge;
        ProFeature wall;
        double from,to;
        ProName name;
        swprintf_s(name,PRO_NAME_SIZE,L"%ls_%d",tx->name,(i-9)/2+1);
        error=cm_interval_edge(tx->model,&original,as_arg(cmd,i)*tx->scale,as_arg(cmd,i+1)*tx->scale,0.001*tx->scale,&edge,&from,&to);
        if(!error)error=cm_attach(tx,name,&edge,from,to,as_arg(cmd,4),as_arg(cmd,5),as_arg(cmd,6),cmd->arg[7],&wall);
        if(error)return error;
    }
    part_text_add(out,L"已沿同一板边建立 %d 段独立翻边，直段与折弯范围均已读回核对。",(cmd->count-9)/2);
    return PRO_TK_NO_ERROR;
}
