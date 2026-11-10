static void qp_spacer(QuickPlan *p,const double *v){
    p->depth=p->width=v[0];p->thickness=v[3];
    p->step_diameter=v[1];p->bore=v[2];p->step_height=v[4];
}
