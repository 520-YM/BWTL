static ProError mp_groove_profile(const PanelSketch *sketch,const PanelProfile *p){
    double x0=p->x,y0=p->y,x1=x0+p->width,y1=y0+p->height,r=p->detail;
    ProError error;
    if((error=pn_line(sketch,x0+r,y0,x1,y0))||
       (error=pn_line(sketch,x1,y0,x1,y1))||
       (error=pn_line(sketch,x1,y1,x0+r,y1))||
       (error=pn_arc(sketch,x0+r,y1-r,r,AS_PI/2,AS_PI/2))||
       (error=pn_line(sketch,x0,y1-r,x0,y0+r)))return error;
    return pn_arc(sketch,x0+r,y0+r,r,AS_PI,AS_PI/2);
}
