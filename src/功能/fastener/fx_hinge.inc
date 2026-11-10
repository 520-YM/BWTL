static void fx_hinge(FastenerPlan *plan,const PartCommand *cmd,double x,double y){
    double pitch_x=as_arg(cmd,10),pitch_y=as_arg(cmd,11),d=as_arg(cmd,13);
    int rows=(int)as_arg(cmd,12);
    for(int i=0;i<rows;i++){
        double row=y+(i-(rows-1)/2.0)*pitch_y;
        if(pitch_x)pn_hole_pair(&plan->panel,x,row,pitch_x,d);
        else pn_circle(&plan->panel,x,row,d);
    }
    plan->margin=as_arg(cmd,14);
}
