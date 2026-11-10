/* Offsets are relative to the chosen face, looking inward through the panel. */
static ProError pn_probe(FdTxn *tx,const PanelFrame *panel,AsPoint point,double front,double back,int source_face){
    ProRay line;
    QualityRay hits;
    double epsilon=0.0001*tx->scale;
    am_world(&panel->placement,point.x*tx->scale,point.y*tx->scale,0,line.start_point);
    memcpy(line.dir_vector,panel->inward,sizeof(line.dir_vector));
    ProError error=qa_ray(tx->model,&line,epsilon,&hits);
    if(error)return error;
    if(back==0)return hits.count?PRO_TK_BAD_CONTEXT:PRO_TK_NO_ERROR;
    if(hits.count!=2||fabs(hits.crossing[0].depth-front*tx->scale)>epsilon||
       fabs(hits.crossing[1].depth-back*tx->scale)>epsilon||
       (source_face&&hits.crossing[0].surface_id!=panel->face_id))return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}

static ProError pn_profile_probes(FdTxn *tx,const PanelFrame *panel,const PanelProfile *p,double thickness,int after){
    double w=p->width,h=p->height,x=p->x,y=p->y;
    AsPoint points[9]={{x,y},{x-w*0.4,y},{x+w*0.4,y},{x,y-h*0.4},{x,y+h*0.4}};
    int count=5;
    if(p->shape==PN_KEY||p->shape==PN_CAPSULE){
        AsPoint axis=as_ray(p->angle),side=as_left(axis),center=as_point(x,y);
        double cross=p->shape==PN_KEY?w:h;
        points[1]=as_plus(center,as_mul(axis,-w*0.4));
        points[2]=as_plus(center,as_mul(axis,w*0.4));
        points[3]=as_plus(center,as_mul(side,cross*0.4));
        points[4]=as_plus(center,as_mul(side,-cross*0.4));
        if(p->shape==PN_KEY){
            AsPoint tip=as_plus(center,as_mul(axis,p->detail));
            points[count++]=as_plus(tip,as_mul(axis,h*0.35));
            points[count++]=as_plus(tip,as_mul(side,h*0.35));
            points[count++]=as_plus(tip,as_mul(side,-h*0.35));
        }
    }
    if(p->shape==PN_FLAT)points[2].x=x+p->detail*0.8;
    if(p->shape==PN_NOTCH){
        points[3].y=y-w*0.4;
        points[4].y=y+w*0.4;
        points[count++]=as_point(x+w/2+p->detail/2,y);
    }
    if(p->shape==PN_EDGE_SLOT){
        points[0].x=x+w/2;
        points[1].x=x+w*0.1;
        points[2].x=x+w*0.9;
        points[3].x=points[4].x=x+w/2;
        AsPoint outside=as_point(x-fmax(0.05,fmin(w,h)/8),y);
        ProError error=pn_probe(tx,panel,outside,0,0,0);
        if(error)return error;
    }
    for(int i=0;i<count;i++){
        ProError error=pn_probe(tx,panel,points[i],0,after?0:thickness,!after);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError pn_seat_probes(FdTxn *tx,const PanelFrame *panel,const PanelPlan *plan,int after){
    const PanelProfile *seat=&plan->seat,*inner=&plan->through[0];
    double x=seat->x,y=seat->y,dx=(seat->width+inner->width)/4,dy=(seat->height+inner->height)/4;
    AsPoint points[]={{x-dx,y},{x+dx,y},{x,y-dy},{x,y+dy}};
    for(int i=0;i<_countof(points);i++){
        ProError error=pn_probe(tx,panel,points[i],after?plan->seat_depth:0,plan->thickness,!after);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError pn_display_support(FdTxn *tx,const PanelFrame *panel,const PanelPlan *plan){
    const PanelProfile *outline=&plan->seat;
    double dx=outline->width/2-fmin(0.05,plan->bezel_x/4);
    double dy=outline->height/2-fmin(0.05,plan->bezel_y/4);
    for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
        if(!x&&!y)continue;
        AsPoint point=as_point(outline->x+x*dx,outline->y+y*dy);
        ProError error=pn_probe(tx,panel,point,0,plan->thickness,1);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}
