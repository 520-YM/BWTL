static ProError fx_profile_draw(const PanelSketch *source,const PanelProfile *profile){
    PanelSketch sketch=*source;
    AsPoint axis=as_ray(profile->angle);
    sketch.origin=pn_project(source,profile->x,profile->y);
    sketch.x_axis=as_plus(as_mul(source->x_axis,axis.x),as_mul(source->y_axis,axis.y));
    sketch.y_axis=as_plus(as_mul(source->x_axis,-axis.y),as_mul(source->y_axis,axis.x));
    ProError error;
    if(profile->shape==PN_HEX){
        double radius=profile->width/sqrt(3.0);
        for(int i=0;i<6;i++){
            AsPoint a=as_mul(as_ray((i+0.5)*AS_PI/3),radius);
            AsPoint b=as_mul(as_ray((i+1.5)*AS_PI/3),radius);
            if((error=pn_line(&sketch,a.x,a.y,b.x,b.y)))return error;
        }
        return PRO_TK_NO_ERROR;
    }
    double r=profile->height/2;
    if(profile->shape==PN_CAPSULE){
        double half=(profile->width-profile->height)/2;
        if((error=pn_line(&sketch,-half,-r,half,-r))||
           (error=pn_arc(&sketch,half,0,r,-AS_PI/2,AS_PI))||
           (error=pn_line(&sketch,half,r,-half,r)))return error;
        return pn_arc(&sketch,-half,0,r,AS_PI/2,AS_PI);
    }
    double radius=profile->width/2,angle=asin(r/radius),join=radius*cos(angle),travel=profile->detail;
    if((error=pn_arc(&sketch,0,0,radius,angle,2*(AS_PI-angle)))||
       (error=pn_line(&sketch,join,-r,travel,-r))||
       (error=pn_arc(&sketch,travel,0,r,-AS_PI/2,AS_PI)))return error;
    return pn_line(&sketch,travel,r,join,r);
}
