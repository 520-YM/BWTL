static ProError quick_step(FdTxn *tx,const QuickPlan *plan){
    PartCommand cmd={0};
    PanelFrame frame={0};
    ProMatrix location;
    ProMassProperty mass;
    ProError error=quick_frame(tx,plan->thickness,&cmd,&frame);
    if(!error)error=qa_geometry_mass(tx->model,&mass);
    if(error)return error;
    tx->volume=mass.volume;
    const double center=plan->width/2;
    const PanelProfile profile[]={
        {PN_CIRCLE,center,center,plan->step_diameter,plan->step_diameter,0,0},
        {PN_CIRCLE,center,center,plan->bore,plan->bore,0,0}};
    error=pn_sketch(tx->model,&cmd,&frame,L"QP_STEP_SK",profile,_countof(profile),0,location);
    if(!error)error=md_extrude(tx,L"QP_STEP",L"QP_STEP_SK",location,frame.placement.csys.z_vector,plan->step_height,0);
    if(!error)error=md_expected_volume(tx,(pn_area(&profile[0])-pn_area(&profile[1]))*plan->step_height);
    if(!error){
        ProRay ray={0};
        QualityRay hits;
        ray.start_point[0]=ray.start_point[1]=center*tx->scale;
        ray.start_point[2]=(plan->thickness+plan->step_height+1)*tx->scale;
        ray.dir_vector[2]=-1;
        error=qa_ray(tx->model,&ray,0.00001*tx->scale,&hits);
        if(!error&&hits.count)error=PRO_TK_BAD_CONTEXT;
    }
    return error;
}

typedef struct {
    double center,scale,radius[3],low[3],high[3];
    int seen[3],kinds;
} QuickCylinders;

static ProError quick_cylinder_visit(ProSurface surface,ProError status,ProAppData data){
    QuickCylinders *s=data;
    if(status)return status;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    if((error=ProSurfaceDataGet(surface,&raw)))return error;
    const ProCylinderdata *c=&raw->data.p_surface_data->srf_shape.cylinder;
    const double tol=0.0001*s->scale;
    int k=-1;
    if(fabs(c->origin[0]-s->center)<tol&&fabs(c->origin[1]-s->center)<tol&&fabs(c->e3[2])>1-1e-8)
        for(int i=0;i<s->kinds;i++)if(fabs(c->radius-s->radius[i])<tol){k=i;break;}
    ProGeomitemdataFree(&raw);
    if(k<0)return PRO_TK_BAD_CONTEXT;
    ProVector axis={0,0,1},lower,upper;
    if((error=ProSurfaceExtremesEval(surface,axis,lower,upper)))return error;
    s->low[k]=fmin(s->low[k],lower[2]);s->high[k]=fmax(s->high[k],upper[2]);s->seen[k]++;
    return PRO_TK_NO_ERROR;
}

static ProError quick_spacer_check(const FdTxn *tx,const QuickPlan *plan,int with_step){
    ProSolidBody body;
    ProError error=qa_single_body(tx->model,&body);
    if(error)return error;
    QuickCylinders s={0};
    s.scale=tx->scale;s.kinds=with_step?3:2;
    s.center=plan->width/2*tx->scale;
    s.radius[0]=s.center;
    s.radius[1]=plan->bore/2*tx->scale;
    s.radius[2]=plan->step_diameter/2*tx->scale;
    for(int i=0;i<s.kinds;i++){s.low[i]=HUGE_VAL;s.high[i]=-HUGE_VAL;}
    if((error=ProSolidBodySurfaceVisit(&body,quick_cylinder_visit,&s)))return error;
    const double low[]={0,0,plan->thickness};
    const double high[]={plan->thickness,plan->thickness+(with_step?plan->step_height:0),plan->thickness+plan->step_height};
    for(int i=0;i<s.kinds;i++){
        if(!s.seen[i]||fabs(s.low[i]/tx->scale-low[i])>0.001||fabs(s.high[i]/tx->scale-high[i])>0.001)
            return PRO_TK_BAD_CONTEXT;
    }
    return PRO_TK_NO_ERROR;
}
