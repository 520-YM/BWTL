static void pn_button(PanelPlan *plan,double x,double y,double diameter,double notch_width,double notch_depth){
    if(notch_width){
        plan->through[plan->count++]=(PanelProfile){PN_NOTCH,x,y,diameter,notch_width,notch_depth};
    }else pn_circle(plan,x,y,diameter);
}
