static void qp_l(QuickPlan *p,const double *v){
    p->walls=1;
    p->height=v[3];p->radius=v[4];
    quick_pair(p,v[5],v[6]);
}
