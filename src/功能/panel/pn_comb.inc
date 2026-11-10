static void pn_comb(PanelPlan *plan,double x,double y,const double size[5]){
    int count=(int)size[0];
    for(int i=0;i<count;i++){
        double center=y+(i-(count-1)/2.0)*size[2];
        plan->through[plan->count++]=(PanelProfile){PN_EDGE_SLOT,x,center,size[3],size[1],0};
    }
}
