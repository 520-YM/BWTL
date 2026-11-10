static void pn_handle(PanelPlan *plan,double x,double y,const double size[5]){
    /* Two-hole handle: the final pitch is zero. Four-hole: rectangular spacing. */
    if(size[2])pn_hole_rectangle(plan,x,y,size[0],size[2],size[1]);
    else pn_hole_pair(plan,x,y,size[0],size[1]);
}
