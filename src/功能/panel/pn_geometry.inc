static PanelProfile pn_rect(double x,double y,double width,double height){
    return (PanelProfile){PN_RECTANGLE,x,y,width,height,0};
}

static void pn_circle(PanelPlan *plan,double x,double y,double diameter){
    plan->through[plan->count++]=(PanelProfile){PN_CIRCLE,x,y,diameter,diameter,0};
}

static void pn_hole_pair(PanelPlan *plan,double x,double y,double pitch,double diameter){
    pn_circle(plan,x-pitch/2,y,diameter);
    pn_circle(plan,x+pitch/2,y,diameter);
}

static void pn_hole_rectangle(PanelPlan *plan,double x,double y,double pitch_x,double pitch_y,double diameter){
    pn_hole_pair(plan,x,y-pitch_y/2,pitch_x,diameter);
    pn_hole_pair(plan,x,y+pitch_y/2,pitch_x,diameter);
}

static double pn_area(const PanelProfile *profile){
    double radius=profile->width/2;
    if(profile->shape==PN_KEY){
        double r=profile->height/2,join=sqrt(radius*radius-r*r);
        return radius*radius*(AS_PI-asin(r/radius))+join*r+
            2*r*(profile->detail-join)+AS_PI*r*r/2;
    }
    if(profile->shape==PN_CAPSULE){
        double r=profile->height/2;
        return AS_PI*r*r+(profile->width-profile->height)*profile->height;
    }
    if(profile->shape==PN_HEX)return sqrt(3.0)*profile->width*profile->width/2;
    if(profile->shape==PN_CIRCLE)return AS_PI*radius*radius;
    if(profile->shape==PN_ROUNDED)
        return profile->width*profile->height-(4-AS_PI)*profile->detail*profile->detail;
    if(profile->shape==PN_FLAT){
        double x=profile->detail;
        return radius*radius*(AS_PI-acos(x/radius))+x*sqrt(radius*radius-x*x);
    }
    if(profile->shape==PN_NOTCH){
        double h=profile->height/2,x=sqrt(radius*radius-h*h);
        return radius*radius*(AS_PI-asin(h/radius))+x*h+(radius+profile->detail-x)*2*h;
    }
    return profile->width*profile->height;
}

static void pn_bounds(const PanelProfile *p,double bounds[4]){
    bounds[0]=p->x-p->width/2;
    bounds[1]=p->x+p->width/2;
    bounds[2]=p->y-p->height/2;
    bounds[3]=p->y+p->height/2;
    if(p->shape==PN_FLAT)bounds[1]=p->x+p->detail;
    if(p->shape==PN_NOTCH){
        bounds[1]+=p->detail;
        bounds[2]=p->y-p->width/2;
        bounds[3]=p->y+p->width/2;
    }
    if(p->shape==PN_EDGE_SLOT){
        bounds[0]=p->x;
        bounds[1]=p->x+p->width;
    }
}

static int pn_profiles_separate(const PanelPlan *plan){
    for(int i=0;i<plan->count;i++)for(int j=0;j<i;j++){
        const PanelProfile *a=&plan->through[i],*b=&plan->through[j];
        if(a->shape==PN_CIRCLE&&b->shape==PN_CIRCLE){
            if(hypot(a->x-b->x,a->y-b->y)<(a->width+b->width)/2+0.01)return 0;
        }else{
            double aa[4],bb[4];
            pn_bounds(a,aa);pn_bounds(b,bb);
            if(aa[1]+0.01>bb[0]&&bb[1]+0.01>aa[0]&&aa[3]+0.01>bb[2]&&bb[3]+0.01>aa[2])return 0;
        }
    }
    return 1;
}
