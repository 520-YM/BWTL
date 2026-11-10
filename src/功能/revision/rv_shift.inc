static ProError rv_shift(Revision *tx,const PartCommand *cmd,PartText *out){
    RevisionHole holes[8];
    int n=(int)as_arg(cmd,4);
    ProError error=rv_load_group(tx,cmd,5,n,holes);
    for(int i=0;!error&&i<n;i++)
        error=rv_hole_position(tx,&holes[i],holes[i].x+as_arg(cmd,2)/tx->mm,holes[i].y+as_arg(cmd,3)/tx->mm);
    if(!error)error=rv_finish_holes(tx,holes,n);
    if(!error)part_text_add(out,L"已平移 %d 个原生孔，按共同定位基准偏移 %.8g、%.8g mm。",n,as_arg(cmd,2),as_arg(cmd,3));
    return error;
}
