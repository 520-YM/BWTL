static void qp_stop(QuickPlan *p,const double *v){
    quick_opening(p,Q_SLOT,p->width/2,(p->depth-v[5])/2,v[3],v[4]);
    quick_opening(p,Q_SLOT,p->width/2,(p->depth+v[5])/2,v[3],v[4]);
}
