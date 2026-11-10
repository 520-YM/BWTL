static void pn_fan(PanelPlan *plan,double x,double y,const double size[5]){
    pn_circle(plan,x,y,size[0]);
    pn_hole_rectangle(plan,x,y,size[1],size[2],size[3]);
}
