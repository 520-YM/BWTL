static void fx_corners(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double a=as_arg(cmd,10),b=as_arg(cmd,11);
    if(!wcscmp(cmd->arg[9],L"CENTER")){
        pn_hole_rectangle(&plan->panel,x,y,a,b,as_arg(cmd,12));
        plan->margin=as_arg(cmd,13);
    }else{
        plan->box_width=a;
        plan->box_height=b;
        pn_hole_rectangle(&plan->panel,x+a/2,y+b/2,a-2*as_arg(cmd,12),b-2*as_arg(cmd,13),as_arg(cmd,14));
        plan->margin=as_arg(cmd,15);
    }
}
