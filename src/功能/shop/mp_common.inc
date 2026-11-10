typedef struct {
    PartCommand sketch_command;
    PanelFrame frame;
} ShopSetup;

typedef struct {
    PanelFrame frame;
    QualityBore bore;
    CabinetRing ring;
    double radius;
} ShopMouth;

static ProError mp_setup(FdTxn *tx,const PartCommand *cmd,int at,ShopSetup *setup){
    memset(setup,0,sizeof(*setup));
    for(int i=0;i<3;i++)wcscpy_s(setup->sketch_command.arg[3+i],PART_TOKEN_MAX,cmd->arg[at+i]);
    return pn_frame(tx,&setup->sketch_command,&setup->frame);
}

static ProError mp_read_stock(FdTxn *tx,const PanelFrame *frame,AsPoint p,double front,double *back,int original){
    ProRay ray;
    QualityRay hits;
    am_world(&frame->placement,p.x*tx->scale,p.y*tx->scale,0,ray.start_point);
    memcpy(ray.dir_vector,frame->inward,sizeof(ProVector));
    ProError error=qa_ray(tx->model,&ray,0.0001*tx->scale,&hits);
    if(error)return error;
    if(hits.count!=2||!hits.crossing[0].entering||fabs(hits.crossing[0].depth-front*tx->scale)>0.0002*tx->scale||
       (original&&hits.crossing[0].surface_id!=frame->face_id)||hits.crossing[1].depth<=hits.crossing[0].depth)
        return PRO_TK_BAD_CONTEXT;
    if(back)*back=hits.crossing[1].depth/tx->scale;
    return PRO_TK_NO_ERROR;
}

static ProError mp_probe_min(FdTxn *tx,const PanelFrame *frame,AsPoint p,double front,double minimum_back,int original){
    double back;
    ProError error=mp_read_stock(tx,frame,p,front,&back,original);
    return error?error:back+0.0001>=minimum_back?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError mp_circle_probes(FdTxn *tx,const PanelFrame *frame,AsPoint center,double radius,
    double front,double min_back,int original){
    for(int i=0;i<8;i++){
        AsPoint p=as_plus(center,as_mul(as_ray((i+0.25)*AS_PI/4),radius));
        ProError error=mp_probe_min(tx,frame,p,front,min_back,original);
        if(error)return error;
    }
    return PRO_TK_NO_ERROR;
}

static ProError mp_prepare_profile(FdTxn *tx,const ShopSetup *setup,const wchar_t *suffix,const PanelProfile *profiles,
    int count,ProName sketch,ProMatrix matrix){
    swprintf_s(sketch,PRO_NAME_SIZE,L"%ls_%ls",tx->name,suffix);
    return pn_sketch(tx->model,&setup->sketch_command,&setup->frame,sketch,profiles,count,0,matrix);
}

static ProError mp_blind(FdTxn *tx,const ShopSetup *setup,const wchar_t *suffix,const PanelProfile *profiles,
    int count,double depth){
    ProName sketch;
    ProMatrix matrix;
    ProError error=mp_prepare_profile(tx,setup,suffix,profiles,count,sketch,matrix);
    return error?error:pn_cut(tx,&setup->frame,tx->name,sketch,matrix,depth,0);
}

static ProError mp_rim_wall(FdTxn *tx,ProSurface surface,const QualityBore *selected){
    ProGeomitemdata *raw=NULL;
    ProError error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    const ProSurfacedata *data=raw->data.p_surface_data;
    if(data->type!=PRO_SRF_CYL)error=PRO_TK_BAD_CONTEXT;
    else{
        const ProCylinderdata *wall=&data->srf_shape.cylinder;
        ProVector offset,radial;
        ad_sub(wall->origin,selected->origin,offset);
        ad_cross(offset,selected->axis,radial);
        if(fabs(ad_dot(wall->e3,selected->axis))<1-1e-8||
           !cm_near(wall->radius,selected->radius,tx->scale)||
           !cm_near(sqrt(ad_dot(radial,radial)),0,tx->scale))error=PRO_TK_BAD_CONTEXT;
    }
    ProGeomitemdataFree(&raw);
    return error;
}

static ProError mp_mouth(FdTxn *tx,const wchar_t *face_token,const wchar_t *bore_token,ShopMouth *mouth){
    FdRef face={0},bore={0};
    ProSurface surface;
    ProVector plane,normal,delta;
    ProError error=md_solid_face(tx,face_token,&face);
    if(!error)error=fd_plane_data(&face.item,plane,normal);
    if(!error)error=ProGeomitemToSurface(&face.item,&surface);
    if(!error)error=qa_bore(tx->model,bore_token,&bore,&mouth->bore);
    if(error)goto done;
    if(!ad_unit(normal)||fabs(ad_dot(normal,mouth->bore.axis))<1-1e-8){error=PRO_TK_BAD_CONTEXT;goto done;}
    ProCsysdata *csys=&mouth->frame.placement.csys;
    ad_sub(plane,mouth->bore.origin,delta);
    cm_point(mouth->bore.origin,normal,ad_dot(delta,normal),csys->origin);
    memcpy(csys->z_vector,normal,sizeof(normal));
    ProVector seed={1,0,0};
    if(fabs(normal[0])>0.8){seed[0]=0;seed[1]=1;}
    ad_cross(seed,normal,csys->y_vector);
    if(!ad_unit(csys->y_vector)){error=PRO_TK_BAD_CONTEXT;goto done;}
    ad_cross(csys->y_vector,normal,csys->x_vector);
    for(int i=0;i<3;i++)mouth->frame.inward[i]=-normal[i];
    mouth->frame.face_id=face.item.id;
    mouth->radius=mouth->bore.radius/tx->scale;
    CabinetRing *ring=&mouth->ring;
    ring->model=tx->model;
    ring->face=surface;
    ring->scale=tx->scale;
    ring->radius=mouth->bore.radius;
    memcpy(ring->center,csys->origin,sizeof(ProVector));
    memcpy(ring->axis,normal,sizeof(ProVector));
    error=ProSurfaceContourVisit(surface,cm_ring_contour,NULL,ring);
    if(!error&&(!ring->count||!cm_near(ring->length,2*AS_PI*ring->radius,tx->scale)))error=PRO_TK_BAD_CONTEXT;
    int selected=0;
    for(int i=0;!error&&i<ring->count;i++){
        ProEdge edge,next[2];
        ProSurface adjacent[2];
        int ids[2];
        if((error=ProEdgeInit((ProSolid)tx->model,ring->ids[i],&edge))||
           (error=ProEdgeNeighborsGet(edge,&next[0],&next[1],&adjacent[0],&adjacent[1]))||
           (error=ProSurfaceIdGet(adjacent[0],&ids[0]))||(error=ProSurfaceIdGet(adjacent[1],&ids[1])))break;
        int wall=ids[0]==face.item.id?1:ids[1]==face.item.id?0:-1;
        if(wall<0){error=PRO_TK_BAD_CONTEXT;break;}
        selected|=ids[wall]==bore.item.id;
        error=mp_rim_wall(tx,adjacent[wall],&mouth->bore);
    }
    if(!error&&!selected)error=PRO_TK_BAD_CONTEXT;
done:
    fd_ref_free(&face);
    fd_ref_free(&bore);
    return error;
}

static PanelProfile mp_circle(AsPoint center,double diameter){
    return (PanelProfile){PN_CIRCLE,center.x,center.y,diameter,diameter,0,0};
}
