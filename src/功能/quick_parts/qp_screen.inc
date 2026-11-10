static void qp_screen(QuickPlan *p,const double *v){
    quick_grid(p,v[5],v[6],v[7]);
    quick_opening(p,Q_RECT,p->width/2,p->depth/2,v[3],v[4]);
}
