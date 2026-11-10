#ifndef BWTL_ADVANCED_SKETCH_SHAPES_H
#define BWTL_ADVANCED_SKETCH_SHAPES_H

#define AS_PI 3.14159265358979323846
typedef struct {double x,y;} AsPoint;
static AsPoint as_point(double x,double y){AsPoint p={x,y};return p;}
static AsPoint as_p(const double *xy){return as_point(xy[0],xy[1]);}
static AsPoint as_plus(AsPoint a,AsPoint b){return as_point(a.x+b.x,a.y+b.y);}
static AsPoint as_minus(AsPoint a,AsPoint b){return as_point(a.x-b.x,a.y-b.y);}
static AsPoint as_mul(AsPoint a,double s){return as_point(a.x*s,a.y*s);}
static AsPoint as_left(AsPoint a){return as_point(-a.y,a.x);}
static double as_cross2(AsPoint a,AsPoint b){return a.x*b.y-a.y*b.x;}
static double as_dot2(AsPoint a,AsPoint b){return a.x*b.x+a.y*b.y;}
static double as_dist(AsPoint a,AsPoint b){return hypot(a.x-b.x,a.y-b.y);}
static AsPoint as_ray(double radians){return as_point(cos(radians),sin(radians));}
static void as_xy(double *xy,AsPoint p){xy[0]=p.x;xy[1]=p.y;}
static double as_arg(const PartCommand *cmd,int i){double d=0;part_number(cmd->arg[i],&d);return d;}

static ProError as_add_line(ProSection section,AsPoint a,AsPoint b,int *id){
    Pro2dLinedef line={0};
    line.type=PRO_2D_LINE;as_xy(line.end1,a);as_xy(line.end2,b);
    return ProSectionEntityAdd(section,(Pro2dEntdef *)&line,id);
}

static ProError as_add_arc(ProSection section,AsPoint center,double radius,double start,double end,int *id){
    Pro2dArcdef arc={0};
    arc.type=PRO_2D_ARC;as_xy(arc.center,center);arc.radius=radius;
    while(start<0){start+=2*AS_PI;end+=2*AS_PI;}
    while(start>=2*AS_PI){start-=2*AS_PI;end-=2*AS_PI;}
    arc.start_angle=start;arc.end_angle=end;
    return ProSectionEntityAdd(section,(Pro2dEntdef *)&arc,id);
}

static ProError as_add_bspline(ProSection section,int degree,Pro2dPnt *points,int n,const double *knots,const double *weights,int *id){
    Pro2dBSplinedef def={0};
    ProError e;
    int knot_count=n+degree+1;
    def.type=PRO_2D_B_SPLINE;def.degree=degree;def.num_c_points=n;def.num_knots=knot_count;
    if((e=ProArrayAlloc(n,sizeof(Pro2dPnt),1,(ProArray *)&def.c_pnts))||
       (e=ProArrayAlloc(knot_count,sizeof(double),1,(ProArray *)&def.params)))goto done;
    memcpy(def.c_pnts,points,n*sizeof(Pro2dPnt));
    memcpy(def.params,knots,knot_count*sizeof(double));
    if(weights){
        if((e=ProArrayAlloc(n,sizeof(double),1,(ProArray *)&def.weights)))goto done;
        memcpy(def.weights,weights,n*sizeof(double));
    }
    e=ProSectionEntityAdd(section,(Pro2dEntdef *)&def,id);
done:
    if(def.weights)ProArrayFree((ProArray *)&def.weights);
    if(def.params)ProArrayFree((ProArray *)&def.params);
    if(def.c_pnts)ProArrayFree((ProArray *)&def.c_pnts);
    return e;
}

#include "功能/advanced_sketch/as_ellipse.inc"
#include "功能/advanced_sketch/as_slot.inc"
#include "功能/advanced_sketch/as_polygon.inc"
#include "功能/advanced_sketch/as_spline.inc"

static ProError as_shape_append(ProSection section,const PartCommand *cmd,int first,int kind,double scale,int *ids,int *count){
    switch(kind){
    case AS_ELLIPSE:*count=1;return as_ellipse(section,cmd,first,scale,ids);
    case AS_SLOT:*count=4;return as_slot(section,cmd,first,scale,ids);
    case AS_POLYGON:return as_polygon(section,cmd,first,scale,ids,count);
    case AS_SPLINE:*count=1;return as_spline(section,cmd,first,scale,ids);
    }
    return PRO_TK_BAD_INPUTS;
}
#endif
