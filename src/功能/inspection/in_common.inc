typedef struct {ProMdl model;ProSolidBody body;double mm;} Inspection;

typedef struct {ProSurface handle;ProVector point,normal;int id;} InspectionPlane;

static ProError in_plane_read(ProMdl model,ProSurface surface,InspectionPlane *plane){
    ProGeomitem item;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(surface,&type);
    if(error||type!=PRO_SRF_PLANE)return error?error:PRO_TK_INVALID_TYPE;
    plane->handle=surface;
    if((error=ProSurfaceToGeomitem((ProSolid)model,surface,&item))||
       (error=fd_plane_data(&item,plane->point,plane->normal)))return error;
    plane->id=item.id;
    return ad_unit(plane->normal)?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}

static ProError in_plane(ProMdl model,const wchar_t *token,InspectionPlane *plane){
    FdRef ref={0};
    ProSurface face;
    ProError error=qa_surface(model,token,1,&ref,&face);
    if(!error)error=in_plane_read(model,face,plane);
    fd_ref_free(&ref);
    return error;
}

static int in_on_plane(ProMdl model,const InspectionPlane *plane,const double *point){
    ProUvParam uv;
    ProUvStatus status;
    return !ProSurfaceParamEval((ProSolid)model,plane->handle,(double *)point,uv)&&
        !ProSurfaceUvpntVerify((ProSolid)model,plane->handle,uv,&status)&&status==PRO_UV_INSIDE;
}

static ProError in_edge(ProMdl model,const wchar_t *token,ProEdge *edge){
    FdRef ref={0};
    ProError error=fd_ref_resolve(model,token,PRO_EDGE,&ref);
    if(!error)error=ProGeomitemToEdge(&ref.item,edge);
    fd_ref_free(&ref);
    return error;
}

static double in_segment_distance(const double *p,const double *a,const double *b){
    ProVector ab,ap,closest;
    ad_sub(b,a,ab);ad_sub(p,a,ap);
    double squared=ad_dot(ab,ab);
    if(squared<=0)return cm_distance(p,a);
    cm_point(a,ab,qa_clamp(2*ad_dot(ap,ab)/squared-1)*0.5+0.5,closest);
    return cm_distance(p,closest);
}

static ProError in_mouth(const Inspection *inspect,const wchar_t *face,const wchar_t *bore,ShopMouth *mouth){
    FdTxn view={0};
    view.model=inspect->model;
    view.scale=1/inspect->mm;
    return mp_mouth(&view,face,bore,mouth);
}

static ProError in_native_hole(const Inspection *inspect,const wchar_t *name,int kind,RevisionHole *hole){
    Revision view={0};
    view.model=inspect->model;
    view.mm=inspect->mm;
    ProError error=rv_hole_load(&view,name,kind,0,0,hole);
    return error?error:rv_hole_check(&view,hole);
}

static ProError in_blind(const Inspection *inspect,const wchar_t *name,RevisionHole *hole,double *bottom){
    ProError error=in_native_hole(inspect,name,RV_STRAIGHT,hole);
    if(error||!hole->blind)return error?error:PRO_TK_BAD_CONTEXT;
    double plane[]={ad_dot(hole->points[0],hole->normals[0]),ad_dot(hole->points[1],hole->normals[1])+hole->x,
        ad_dot(hole->points[2],hole->normals[2])+hole->y};
    *bottom=INFINITY;
    for(int i=0;i<9;i++){
        ProRay ray={0};
        QualityRay hits;
        double angle=(i+0.25)*AS_PI/4,rad=i==8?0:hole->diameter*0.425;
        for(int j=0;j<3;j++){
            ray.dir_vector[j]=-hole->normals[0][j];
            ray.start_point[j]=plane[0]*hole->normals[0][j]+
                (plane[1]+rad*cos(angle))*hole->normals[1][j]+(plane[2]+rad*sin(angle))*hole->normals[2][j];
        }
        error=qa_ray(inspect->model,&ray,0.0001/inspect->mm,&hits);
        if(error)return error;
        if(hits.count!=2||fabs(hits.crossing[0].depth-hole->depth)>0.001/inspect->mm)return PRO_TK_BAD_CONTEXT;
        ProSurface floor;
        ProGeomitem item;
        ProFeature owner;
        ProSrftype type;
        if((error=ProSurfaceInit((ProSolid)inspect->model,hits.crossing[0].surface_id,&floor))||
           (error=ProSurfaceTypeGet(floor,&type))||(error=ProSurfaceToGeomitem((ProSolid)inspect->model,floor,&item))||
           (error=ProGeomitemFeatureGet(&item,&owner)))return error;
        if(type!=PRO_SRF_PLANE||owner.id!=hole->feature.id)return PRO_TK_BAD_CONTEXT;
        *bottom=fmin(*bottom,(hits.crossing[1].depth-hits.crossing[0].depth)*inspect->mm);
    }
    return PRO_TK_NO_ERROR;
}
