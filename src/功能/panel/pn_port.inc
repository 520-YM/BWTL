static void pn_port(PanelPlan *plan,double x,double y,const double size[5]){
    plan->through[plan->count++]=(PanelProfile){size[2]?PN_ROUNDED:PN_RECTANGLE,x,y,size[0],size[1],size[2]};
}
