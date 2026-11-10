static void pn_lock(PanelPlan *plan,double x,double y,const double size[5]){
    plan->through[plan->count++]=(PanelProfile){PN_FLAT,x,y,size[0],size[0],size[1]};
    if(size[2])pn_hole_pair(plan,x,y,size[2],size[3]);
}
