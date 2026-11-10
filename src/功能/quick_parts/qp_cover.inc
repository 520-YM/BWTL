static void qp_cover(QuickPlan *p,const double *v){
    quick_grid(p,p->width-2*v[3],p->depth-2*v[4],v[5]);
    quick_pair(p,v[6],v[7]);
}
