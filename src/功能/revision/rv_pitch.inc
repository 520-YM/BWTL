static ProError rv_pitch(Revision *tx,const PartCommand *cmd,PartText *out){
    RevisionHole holes[4];
    ProError error=rv_load_group(tx,cmd,4,4,holes);
    if(error)return error;
    double xmin=holes[0].x,xmax=xmin,ymin=holes[0].y,ymax=ymin;
    for(int i=1;i<4;i++){
        xmin=fmin(xmin,holes[i].x);xmax=fmax(xmax,holes[i].x);
        ymin=fmin(ymin,holes[i].y);ymax=fmax(ymax,holes[i].y);
    }
    if((xmax-xmin)*tx->mm<0.1||(ymax-ymin)*tx->mm<0.1)return PRO_TK_BAD_CONTEXT;
    double dx=as_arg(cmd,2)/tx->mm,dy=as_arg(cmd,3)/tx->mm;
    unsigned visited=0;
    for(int i=0;i<4;i++){
        int left=part_close(holes[i].x,xmin),bottom=part_close(holes[i].y,ymin);
        if((!left&&!part_close(holes[i].x,xmax))||(!bottom&&!part_close(holes[i].y,ymax)))return PRO_TK_BAD_CONTEXT;
        unsigned position=1u<<(left+2*bottom);
        if(visited&position)return PRO_TK_BAD_CONTEXT;
        visited|=position;
        error=rv_hole_position(tx,&holes[i],(xmin+xmax+(left?-dx:dx))/2,(ymin+ymax+(bottom?-dy:dy))/2);
        if(error)return error;
    }
    error=rv_finish_holes(tx,holes,4);
    if(!error)part_text_add(out,L"四孔组中心保持，已核对新孔距 %.8g × %.8g mm。",as_arg(cmd,2),as_arg(cmd,3));
    return error;
}
