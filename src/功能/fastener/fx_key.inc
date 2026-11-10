static void fx_key(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    plan->panel.through[plan->panel.count++]=(PanelProfile){
        PN_KEY,x,y,as_arg(cmd,9),as_arg(cmd,10),as_arg(cmd,11),as_arg(cmd,12)*AS_PI/180};
}
