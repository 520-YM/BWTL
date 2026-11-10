static void pn_power(PanelPlan *plan,double x,double y,const double size[5]){
    plan->through[plan->count++]=pn_rect(x,y,size[0],size[1]);
    pn_hole_pair(plan,x,y,size[2],size[3]);
}
