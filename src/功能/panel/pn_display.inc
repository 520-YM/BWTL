static void pn_display(PanelPlan *plan,double x,double y,const double size[5]){
    plan->through[plan->count++]=pn_rect(x,y,size[0],size[1]);
    plan->seat=pn_rect(x,y,size[2],size[3]);
    plan->bezel_x=(size[2]-size[0])/2;
    plan->bezel_y=(size[3]-size[1])/2;
}
