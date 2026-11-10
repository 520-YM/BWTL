static void qp_u(QuickPlan *p,const double *v){
    p->walls=2;
    p->height=v[3];p->radius=v[4];
    quick_pair(p,v[5],v[6]);
}
