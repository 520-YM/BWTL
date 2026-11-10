typedef struct {const AmFrame *frame;double y,area,start,end;int floors,ends;ProMdl model;int feature;} AmGrooveCheck;
static ProError am_groove_face(ProGeomitem *item,ProError status,ProAppData data){
    AmGrooveCheck *c=data;ProVector p,n,d;ProSurface surface;double area,y,z;ProError e;
    if(status)return status;
    if(fd_plane_data(item,p,n))return 0;
    if((e=ProGeomitemToSurface(item,&surface))||(e=ProSurfaceAreaEval(surface,&area)))return e;
    ad_sub(p,c->frame->csys.origin,d);y=ad_dot(d,c->frame->csys.y_vector);z=ad_dot(d,c->frame->csys.z_vector);
    if(fabs(area-c->area)>fmax(1e-6,c->area*1e-5))return 0;
    if(fabs(fabs(ad_dot(n,c->frame->csys.y_vector))-1)<1e-6&&fabs(y-c->y)<1e-5)c->floors++;
    if(fabs(fabs(ad_dot(n,c->frame->csys.z_vector))-1)<1e-6){
        if(fabs(z-c->start)<1e-5)c->ends|=1;
        if(fabs(z-c->end)<1e-5)c->ends|=2;
    }
    return 0;
}
static ProError am_groove_final(ProSurface surface,ProError status,ProAppData data){
    AmGrooveCheck *c=data;ProGeomitem item;ProFeature feature;ProError e;
    if(status)return status;
    if((e=ProSurfaceToGeomitem((ProSolid)c->model,surface,&item))||(e=ProGeomitemFeatureGet(&item,&feature)))return e;
    return feature.id==c->feature?am_groove_face(&item,0,data):0;
}
static ProError am_key_floor(FdTxn *tx,const AmFrame *frame,double y,double width,double length){
    AmGrooveCheck c={0};ProFeature feature;ProError e;
    c.frame=frame;c.y=y;c.area=width*(length-width)+AS_PI*width*width/4;c.model=tx->model;
    if((e=ProModelitemByNameInit(tx->model,PRO_FEATURE,tx->name,(ProModelitem *)&feature)))return e;
    c.feature=feature.id;
    if((e=ProSolidSurfaceVisit((ProSolid)tx->model,am_groove_final,NULL,&c)))return e;
    return c.floors==1?0:PRO_TK_GENERAL_ERROR;
}
static ProError am_ring_geometry(FdTxn *tx,const AmFrame *frame,double start,double width,double depth){
    AmGrooveCheck c={0};ProFeature feature;ProMassProperty mass;ProError e;
    c.frame=frame;c.start=start;c.end=start+width;c.model=tx->model;
    c.area=AS_PI*(frame->internal?pow(frame->radius+depth,2)-pow(frame->radius,2):pow(frame->radius,2)-pow(frame->radius-depth,2));
    if((e=ProModelitemByNameInit(tx->model,PRO_FEATURE,tx->name,(ProModelitem *)&feature)))return e;
    c.feature=feature.id;
    if((e=ProSolidSurfaceVisit((ProSolid)tx->model,am_groove_final,NULL,&c))||
       (e=ProSolidMassPropertyWithDensityGet((ProSolid)tx->model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)))return e;
    return c.ends==3&&fabs(tx->volume-mass.volume-c.area*width)<fmax(1e-6,c.area*width*1e-5)?0:PRO_TK_GENERAL_ERROR;
}
