static ProError rv_outline(Revision *tx,const PartCommand *cmd,int at){
    for(int i=0;i<3;i++)if(!part_close((tx->before[1][i]-tx->before[0][i])*tx->mm,as_arg(cmd,at+i)))
        return PRO_TK_BAD_CONTEXT;
    for(int i=at+7;i<cmd->count;i+=3){
        ProError error=rv_symbol(tx,cmd->arg[i],0,as_arg(cmd,i+1),as_arg(cmd,i+2),0);
        if(error)return error;
    }
    ProError error=rv_apply(tx);
    if(error)return error;
    ProOutline now;
    ProMassProperty mass;
    if((error=rv_geometry(tx->model,now,&mass)))return error;
    for(int i=0;i<3;i++)if(!part_close((now[1][i]-now[0][i])*tx->mm,as_arg(cmd,at+3+i)))return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}
