static ProError rv_diam(Revision *tx,const PartCommand *cmd,PartText *out){
    RevisionHole holes[8];
    int n=(int)as_arg(cmd,4);
    double old=as_arg(cmd,2)/tx->mm,next=as_arg(cmd,3)/tx->mm;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<n;i++){
        RevisionHole *h=&holes[i];
        error=rv_hole_load(tx,cmd->arg[5+i],RV_STRAIGHT,0,0,h);
        if(!error)error=rv_hole_check(tx,h);
        if(!error)error=rv_add(tx,h->dia,h->feature.id,old,next);
        h->diameter=next;
    }
    if(!error)error=rv_finish_holes(tx,holes,n);
    if(!error)part_text_add(out,L"已把 %d 个同规格直孔的直径从 %.8g 改为 %.8g mm。",n,as_arg(cmd,2),as_arg(cmd,3));
    return error;
}
