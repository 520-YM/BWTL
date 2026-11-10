static void qp_power(QuickPlan *p,const double *v){
    p->walls=2;
    p->height=v[3];p->radius=v[4];
    quick_grid(p,v[5],v[6],v[7]);
}
