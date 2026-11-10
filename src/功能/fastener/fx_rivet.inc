static void fx_rivet(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double size=as_arg(cmd,10);
    if(!wcscmp(cmd->arg[9],L"ROUND"))pn_circle(&plan->panel,x,y,size);
    else plan->panel.through[plan->panel.count++]=(PanelProfile){
        PN_HEX,x,y,size,size,0,as_arg(cmd,14)*AS_PI/180};
    plan->min_thick=as_arg(cmd,11);
    plan->max_thick=as_arg(cmd,12);
    plan->margin=as_arg(cmd,13);
}
