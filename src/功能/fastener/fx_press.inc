static void fx_press(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    pn_circle(&plan->panel,x,y,as_arg(cmd,9));
    plan->min_thick=as_arg(cmd,10);
    plan->max_thick=as_arg(cmd,11);
    plan->margin=as_arg(cmd,12);
}
