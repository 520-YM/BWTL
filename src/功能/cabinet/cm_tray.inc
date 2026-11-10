static ProError cm_tray_clearance(const PartCommand *cmd,PartText *out){
    double length=as_arg(cmd,5),width=as_arg(cmd,6),t=as_arg(cmd,7),
        radius=as_arg(cmd,9),gap=as_arg(cmd,10);
    if(gap<radius+t){
        part_text_add(out,L"托盘端部退让 %.8g mm 太小。内弯半径 %.8g mm 加板厚 %.8g mm，退让至少须 %.8g mm。未创建特征。",gap,radius,t,radius+t);
        return PRO_TK_BAD_INPUTS;
    }
    if(fmin(length,width)<=2*gap+radius){
        part_text_add(out,L"底板短边须大于两端退让加内弯半径（%.8g mm）。未创建特征。",2*gap+radius);
        return PRO_TK_BAD_INPUTS;
    }
    return PRO_TK_NO_ERROR;
}

static ProError cm_tray(FdTxn *tx,const PartCommand *cmd,PartText *out){
    CabinetBlank blank={0};
    ProVector along;
    double length=as_arg(cmd,5),width=as_arg(cmd,6),t=as_arg(cmd,7),height=as_arg(cmd,8),
        radius=as_arg(cmd,9),gap=as_arg(cmd,10);
    ProError error=cm_blank_make(tx,cmd,length,width,t,&blank,along);
    if(error)return error;
    double first_height=0;
    for(int i=0;i<4;i++){
        CabinetEdge edge;
        ProFeature wall;
        double from,to;
        ProName name;
        swprintf_s(name,PRO_NAME_SIZE,L"%ls_W%d",tx->name,i+1);
        error=cm_interval_edge(tx->model,&blank.edges[i],gap*tx->scale,
            blank.edges[i].length-gap*tx->scale,0.001*tx->scale,&edge,&from,&to);
        if(!error)error=cm_attach(tx,name,&edge,from,to,height,90,radius,cmd->arg[11],&wall);
        CabinetEdge tip;
        if(!error)error=cm_wall_tip(tx->model,&wall,&edge,&tip);
        if(error)return error;
        double actual=cm_edge_height(&edge,tip.a);
        if(i==0)first_height=actual;
        if(fabs(actual)<0.1*tx->scale||actual*first_height<=0)return PRO_TK_GENERAL_ERROR;
    }
    part_text_add(out,L"已创建四边翻边托盘，底板草绘 %.8g × %.8g mm，每段沿原板边两端各退 %.8g mm。",length,width,gap);
    return PRO_TK_NO_ERROR;
}
