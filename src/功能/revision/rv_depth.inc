static ProError rv_depth(Revision *tx,const PartCommand *cmd,PartText *out){
    RevisionHole holes[8];
    int n=(int)as_arg(cmd,4);
    double old=as_arg(cmd,2)/tx->mm,next=as_arg(cmd,3)/tx->mm;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<n;i++){
        RevisionHole *h=&holes[i];
        error=rv_hole_load(tx,cmd->arg[5+i],RV_STRAIGHT,0,1,h);
        if(!error)error=rv_hole_check(tx,h);
        if(!error)error=rv_add(tx,h->depth_dim,h->feature.id,old,next);
        h->depth=next;
    }
    if(!error)error=rv_finish_holes(tx,holes,n);
    if(!error)part_text_add(out,L"已把 %d 个同规格平底盲孔的深度从 %.8g 改为 %.8g mm，逐孔核对孔底。",n,as_arg(cmd,2),as_arg(cmd,3));
    return error;
}
