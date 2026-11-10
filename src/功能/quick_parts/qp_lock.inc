static void qp_lock(QuickPlan *p,const double *v){
    quick_opening(p,Q_NOTCH,0,p->depth/2,v[6],v[5]);
    quick_opening(p,Q_HOLE,p->width*.75,(p->depth-v[3])/2,v[4],v[4]);
    quick_opening(p,Q_HOLE,p->width*.75,(p->depth+v[3])/2,v[4],v[4]);
}
