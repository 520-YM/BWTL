static ProError rv_recess(Revision *tx,const PartCommand *cmd,PartText *out){
    RevisionHole holes[8];
    int n=(int)as_arg(cmd,7),bore=!wcscmp(cmd->arg[2],L"BORE");
    double old=as_arg(cmd,3)/tx->mm,next=as_arg(cmd,4)/tx->mm,shape_mm=bore?tx->mm:1;
    double oldshape=as_arg(cmd,5)/shape_mm,newshape=as_arg(cmd,6)/shape_mm;
    ProError error=PRO_TK_NO_ERROR;
    for(int i=0;!error&&i<n;i++){
        RevisionHole *h=&holes[i];
        error=rv_hole_load(tx,cmd->arg[8+i],bore?RV_BORE:RV_SINK,0,0,h);
        if(!error&&next<=h->diameter)error=PRO_TK_BAD_INPUTS;
        if(!error)error=rv_hole_check(tx,h);
        if(!error)error=rv_add(tx,h->head_dim,h->feature.id,old,next);
        if(!error)error=rv_add(tx,h->shape_dim,h->feature.id,oldshape,newshape);
        h->head=next;h->shape=newshape;
    }
    if(!error)error=rv_finish_holes(tx,holes,n);
    if(!error)part_text_add(out,L"已更新 %d 个%ls，外径 %.8g mm，%ls %.8g%ls，保留原底孔。",
        n,bore?L"柱形沉孔":L"锥形沉孔",as_arg(cmd,4),bore?L"沉深":L"角度",as_arg(cmd,6),bore?L" mm":L"°");
    return error;
}
