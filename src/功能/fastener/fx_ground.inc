static void fx_ground(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double hole=as_arg(cmd,9),outside=as_arg(cmd,10);
    pn_circle(&plan->panel,x,y,hole);
    plan->marker=(PanelProfile){PN_CIRCLE,x,y,outside,outside,0,0};
    plan->margin=0.05;
}
