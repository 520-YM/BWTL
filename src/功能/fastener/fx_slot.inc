static void fx_capsule(FastenerPlan *plan,double x,double y,double length,double width,double angle){
    plan->panel.through[plan->panel.count++]=(PanelProfile){PN_CAPSULE,x,y,length,width,0,angle};
}
static void fx_slot(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double width=as_arg(cmd,9)+2*as_arg(cmd,10);
    fx_capsule(plan,x,y,width+as_arg(cmd,11),width,as_arg(cmd,12)*AS_PI/180);
}
