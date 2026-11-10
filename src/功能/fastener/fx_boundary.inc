static AsPoint fx_local_xy(const PanelFrame *frame,const double *point,double scale){
    ProVector delta;
    ad_sub(point,frame->placement.csys.origin,delta);
    return as_point(ad_dot(delta,frame->placement.csys.x_vector)/scale,ad_dot(delta,frame->placement.csys.y_vector)/scale);
}

static double fx_segment_distance(AsPoint p,AsPoint a,AsPoint b){
    AsPoint direction=as_minus(b,a);
    double squared=as_dot2(direction,direction);
    if(squared<=0)return as_dist(p,a);
    double t=fmax(0,fmin(1,as_dot2(as_minus(p,a),direction)/squared));
    return as_dist(p,as_plus(a,as_mul(direction,t)));
}

static double fx_positive_angle(double angle){
    double result=fmod(angle,2*AS_PI);
    return result<0?result+2*AS_PI:result;
}

typedef struct {
    ProSurface face;
    const PanelFrame *frame;
    const FastenerPlan *plan;
    double scale,x,y,minimum,reference_min;
    int exterior,outer_count,outer_sides,edge_count,reference,reference_found;
} FastenerBoundary;

static ProError fx_arc_distance(ProEdge edge,const PanelFrame *frame,double scale,
    const ProVector first,const ProVector last,const PanelProfile *p,double *distance){
    ProGeomitemdata *raw=NULL;
    ProError error=ProEdgeDataGet(edge,&raw);
    if(error)return error;
    const ProCurvedata *curve=raw->data.p_curve_data;
    ProVector center,v1,v2;
    double radius=0,length=0;
    if(!curve){error=PRO_TK_BAD_CONTEXT;goto done;}
    int type=curve->arc.type;
    if(type==PRO_ENT_ARC){
        memcpy(center,curve->arc.origin,sizeof(center));
        memcpy(v1,curve->arc.vector1,sizeof(v1));
        memcpy(v2,curve->arc.vector2,sizeof(v2));
        radius=curve->arc.radius;
    }else if(type==PRO_ENT_CIRCLE){
        memcpy(center,curve->circle.center,sizeof(center));
        radius=curve->circle.radius;
    }else {error=PRO_TK_BAD_CONTEXT;goto done;}
    if((error=ProEdgeLengthEval(edge,&length)))goto done;
    if(!isfinite(radius)||radius<=0||!isfinite(length)||length<=0){error=PRO_TK_BAD_CONTEXT;goto done;}
    AsPoint local_center=fx_local_xy(frame,center,scale),target=as_point(p->x,p->y);
    double span=length/radius;
    *distance=fmin(as_dist(target,fx_local_xy(frame,first,scale)),as_dist(target,fx_local_xy(frame,last,scale)));
    if(fabs(span-2*AS_PI)<1e-6){
        *distance=fabs(as_dist(target,local_center)-radius/scale);
        goto done;
    }
    if(type!=PRO_ENT_ARC||!ad_unit(v1)||!ad_unit(v2)||span>=2*AS_PI){error=PRO_TK_BAD_CONTEXT;goto done;}
    ProVector middle,d1,d2,d3,start_delta,mid_delta,end_delta,target_xyz,target_delta;
    if((error=ProEdgeXyzdataEval(edge,0.5,middle,d1,d2,d3)))goto done;
    ad_sub(first,center,start_delta);
    ad_sub(middle,center,mid_delta);
    ad_sub(last,center,end_delta);
    am_world(&frame->placement,p->x*scale,p->y*scale,0,target_xyz);
    ad_sub(target_xyz,center,target_delta);
    double start=atan2(ad_dot(start_delta,v2),ad_dot(start_delta,v1));
    double mid=atan2(ad_dot(mid_delta,v2),ad_dot(mid_delta,v1));
    double end=atan2(ad_dot(end_delta,v2),ad_dot(end_delta,v1));
    double target_angle=atan2(ad_dot(target_delta,v2),ad_dot(target_delta,v1));
    int forward=fabs(fx_positive_angle(end-start)-span)<1e-6&&fx_positive_angle(mid-start)<=span+1e-6;
    int reverse=fabs(fx_positive_angle(start-end)-span)<1e-6&&fx_positive_angle(start-mid)<=span+1e-6;
    if(!forward&&!reverse){error=PRO_TK_BAD_CONTEXT;goto done;}
    if((forward&&fx_positive_angle(target_angle-start)<=span+1e-9)||
       (reverse&&fx_positive_angle(start-target_angle)<=span+1e-9))
        *distance=fabs(as_dist(target,local_center)-radius/scale);
done:
    ProGeomitemdataFree(&raw);
    return error;
}

static double fx_radius_bound(const PanelProfile *profile){
    return profile->width/(profile->shape==PN_HEX?sqrt(3.0):2);
}

static int fx_rectangle_side(const FastenerBoundary *boundary,AsPoint a,AsPoint b){
    double x=boundary->x,y=boundary->y,w=boundary->plan->box_width,h=boundary->plan->box_height;
    double x0=fmin(a.x,b.x),x1=fmax(a.x,b.x),y0=fmin(a.y,b.y),y1=fmax(a.y,b.y),tol=0.0002;
    if(fabs(y1-y0)<tol&&fabs(x0-x)<tol&&fabs(x1-x-w)<tol){
        if(fabs(y0-y)<tol)return 1;
        if(fabs(y0-y-h)<tol)return 2;
    }
    if(fabs(x1-x0)<tol&&fabs(y0-y)<tol&&fabs(y1-y-h)<tol){
        if(fabs(x0-x)<tol)return 4;
        if(fabs(x0-x-w)<tol)return 8;
    }
    return 0;
}

static ProError fx_boundary_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    FastenerBoundary *boundary=data;
    if(++boundary->edge_count>1024)return PRO_TK_BAD_CONTEXT;
    ProEnttype type;
    ProVector first,last,d1,d2,d3;
    int id;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(!error)error=ProEdgeIdGet(edge,&id);
    if(!error)error=ProEdgeXyzdataEval(edge,0,first,d1,d2,d3);
    if(!error)error=ProEdgeXyzdataEval(edge,1,last,d1,d2,d3);
    if(error)return error;
    if(type!=PRO_ENT_LINE&&type!=PRO_ENT_ARC&&type!=PRO_ENT_CIRCLE)return PRO_TK_BAD_CONTEXT;
    AsPoint a=fx_local_xy(boundary->frame,first,boundary->scale),b=fx_local_xy(boundary->frame,last,boundary->scale);
    if(boundary->exterior&&boundary->plan->box_width){
        if(type!=PRO_ENT_LINE)return PRO_TK_BAD_CONTEXT;
        boundary->outer_sides|=fx_rectangle_side(boundary,a,b);
        boundary->outer_count++;
    }
    int selected=boundary->exterior&&id==boundary->reference;
    if(selected){
        if(type!=PRO_ENT_LINE||fabs(a.x-b.x)>0.0002||fabs(a.y-b.y)<0.1)return PRO_TK_BAD_CONTEXT;
        for(int i=0;i<boundary->plan->panel.count;i++){
            double y=boundary->plan->panel.through[i].y;
            if(y<fmin(a.y,b.y)||y>fmax(a.y,b.y))return PRO_TK_BAD_CONTEXT;
        }
        boundary->reference_found=1;
    }
    int count=boundary->plan->marker.width?1:boundary->plan->panel.count;
    for(int i=0;i<count;i++){
        const PanelProfile *profile=boundary->plan->marker.width?&boundary->plan->marker:&boundary->plan->panel.through[i];
        double distance;
        if(type==PRO_ENT_LINE)distance=fx_segment_distance(as_point(profile->x,profile->y),a,b);
        else if((error=fx_arc_distance(edge,boundary->frame,boundary->scale,first,last,profile,&distance)))return error;
        double clearance=distance-fx_radius_bound(profile);
        boundary->minimum=fmin(boundary->minimum,clearance);
        if(selected)boundary->reference_min=fmin(boundary->reference_min,clearance);
    }
    return PRO_TK_NO_ERROR;
}

static ProError fx_boundary_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    FastenerBoundary *boundary=data;
    ProContourTraversal traversal;
    ProError error=ProContourTraversalGet(contour,&traversal);
    if(error)return error;
    boundary->exterior=traversal==PRO_CONTOUR_TRAV_EXTERNAL;
    return ProContourEdgeVisit(boundary->face,contour,fx_boundary_edge,NULL,boundary);
}

static ProError fx_check_boundary(FdTxn *tx,const PartCommand *cmd,const PanelFrame *frame,FastenerPlan *plan){
    FastenerBoundary boundary={0};
    boundary.frame=frame;
    boundary.plan=plan;
    boundary.scale=tx->scale;
    boundary.x=as_arg(cmd,6);
    boundary.y=as_arg(cmd,7);
    boundary.minimum=INFINITY;
    boundary.reference_min=INFINITY;
    if(fx_kind(cmd->arg[0])==FX_HINGE)part_integer(cmd->arg[9]+5,&boundary.reference);
    ProError error=ProSurfaceInit((ProSolid)tx->model,frame->face_id,&boundary.face);
    if(!error)error=ProSurfaceContourVisit(boundary.face,fx_boundary_contour,NULL,&boundary);
    if(error)return error;
    plan->min_actual=boundary.minimum;
    plan->reference_clearance=boundary.reference_min;
    if((boundary.reference&&!boundary.reference_found)||
       (plan->box_width&&(boundary.outer_count!=4||boundary.outer_sides!=15))||
       !isfinite(boundary.minimum)||boundary.minimum+1e-7<plan->margin)return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}
