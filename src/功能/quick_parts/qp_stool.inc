static ProError quick_stool_bottom(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    QuickFace *search=data;
    InspectionPlane plane;
    double area;
    if(!in_plane_read(search->model,surface,&plane)&&plane.normal[2]<-1+1e-8&&
       fabs(plane.point[2])<1e-7&&!ProSurfaceAreaEval(surface,&area)&&area>search->area){
        search->id=plane.id;
        search->area=area;
    }
    return PRO_TK_NO_ERROR;
}

static ProError quick_stool_ray(const FdTxn *tx,double x,double y,double leg_height,double thickness,int leg){
    ProRay ray={0};
    QualityRay hits;
    ray.start_point[0]=x*tx->scale;
    ray.start_point[1]=y*tx->scale;
    ray.start_point[2]=-(leg_height+1)*tx->scale;
    ray.dir_vector[2]=1;
    double tol=0.0001*tx->scale;
    ProError error=qa_ray(tx->model,&ray,tol,&hits);
    if(error)return error;
    if(hits.count!=2||fabs(hits.crossing[0].depth-(leg?1:leg_height+1)*tx->scale)>tol||
       fabs(hits.crossing[1].depth-(leg_height+1+thickness)*tx->scale)>tol)return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}

static ProError quick_stool(FdTxn *tx,const QuickPlan *plan){
    ProSolidBody body;
    ProError error=qa_single_body(tx->model,&body);
    QuickFace search={tx->model,0};
    if(!error)error=ProSolidBodySurfaceVisit(&body,quick_stool_bottom,&search);
    if(error||!search.id)return error?error:PRO_TK_E_NOT_FOUND;
    PartCommand cmd={0};
    PanelFrame frame={0};
    error=quick_frame(tx,plan->thickness,&cmd,&frame);
    if(error)return error;
    frame.placement.csys.origin[2]=0;
    swprintf_s(cmd.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",search.id);
    PanelProfile legs[4];
    const double leg_height=plan->height-plan->thickness;
    const double edge=plan->leg_inset+plan->leg_width/2;
    for(int i=0;i<4;i++)
        legs[i]=pn_rect(i&1?plan->width-edge:edge,i&2?plan->depth-edge:edge,plan->leg_width,plan->leg_width);

    ProMassProperty before;
    if((error=qa_geometry_mass(tx->model,&before)))return error;
    tx->volume=before.volume;
    ProMatrix sketch_frame;
    error=pn_sketch(tx->model,&cmd,&frame,L"QP_STOOL_LEG_LAYOUT",legs,4,0,sketch_frame);
    const ProVector down={0,0,-1};
    if(!error)error=md_extrude(tx,L"QP_STOOL_LEGS",L"QP_STOOL_LEG_LAYOUT",sketch_frame,down,leg_height,0);
    if(!error)error=md_expected_volume(tx,4*plan->leg_width*plan->leg_width*leg_height);
    for(int i=0;!error&&i<4;i++)error=quick_stool_ray(tx,legs[i].x,legs[i].y,leg_height,plan->thickness,1);
    if(!error)error=quick_stool_ray(tx,plan->width/2,plan->depth/2,leg_height,plan->thickness,0);
    return error;
}

static ProError quick_stool_outline(const FdTxn *tx,const QuickPlan *plan,double back_height){
    ProSolidBody body;
    ProOutline bounds;
    ProError error=qa_single_body(tx->model,&body);
    if(!error)error=ProSolidBodyOutlineGet(&body,bounds);
    if(error)return error;
    const double low[]={0,0,plan->thickness-plan->height};
    const double high[]={plan->width,plan->depth,plan->thickness+back_height};
    for(int i=0;i<3;i++)
        if(fabs(bounds[0][i]/tx->scale-low[i])>0.001||fabs(bounds[1][i]/tx->scale-high[i])>0.001)
            return PRO_TK_BAD_CONTEXT;
    return PRO_TK_NO_ERROR;
}
