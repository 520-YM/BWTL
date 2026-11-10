static void fx_tab(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double width=as_arg(cmd,9)-2*as_arg(cmd,10),depth=as_arg(cmd,11),relief=as_arg(cmd,12);
    double offset=(width+relief)/2;
    plan->panel.through[plan->panel.count++]=(PanelProfile){PN_EDGE_SLOT,x,y-offset,depth,relief,0,0};
    plan->panel.through[plan->panel.count++]=(PanelProfile){PN_EDGE_SLOT,x,y+offset,depth,relief,0,0};
}

static ProError fx_tab_material(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,int original){
    double x=as_arg(cmd,6),y=as_arg(cmd,7),t=as_arg(cmd,8),depth=as_arg(cmd,11);
    double half=(as_arg(cmd,9)-2*as_arg(cmd,10))/2,relief=as_arg(cmd,12);
    double offset=fmin(0.1,fmin(depth,half)/4);
    ProError outside=pn_probe(tx,frame,as_point(x-offset,y),0,0,0);
    if(outside)return outside;
    AsPoint points[]={{x+offset,y},{x+depth/2,y-half*0.8},{x+depth/2,y+half*0.8},
        {x+depth+offset,y-half-relief/2},{x+depth+offset,y+half+relief/2}};
    for(int i=0;i<_countof(points);i++){
        ProError error=pn_probe(tx,frame,points[i],0,t,original);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}
