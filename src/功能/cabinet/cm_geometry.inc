typedef struct {
    int id,face,owner;
    ProVector a,b,axis,normal,inward;
    double length;
} CabinetEdge;

typedef struct {
    int id;
    ProVector point,normal;
    double area;
} CabinetFace;

static double cm_distance(const double *a,const double *b){
    ProVector delta;
    ad_sub(a,b,delta);
    return sqrt(ad_dot(delta,delta));
}

static void cm_point(const double *point,const double *axis,double offset,ProVector result){
    for(int i=0;i<3;i++)result[i]=point[i]+axis[i]*offset;
}

static ProError cm_edge_data(ProMdl model,ProEdge edge,CabinetEdge *result){
    ProEnttype type;
    ProSurface surfaces[2],wall=NULL;
    ProGeomitem item;
    ProVector p,d1,d2,d3;
    ProError error=ProEdgeTypeGet(edge,&type);
    if(error)return error;
    if(type!=PRO_ENT_LINE)return PRO_TK_BAD_CONTEXT;
    if((error=ProEdgeNeighborsGet(edge,NULL,NULL,&surfaces[0],&surfaces[1])))return error;
    int side_count=0;
    for(int i=0;i<2;i++){
        ProSmtSurfType side;
        if(!ProSmtSurfaceTypeGet((ProPart)model,surfaces[i],&side)){
            if(side==PRO_SMT_SURF_FACE)wall=surfaces[i];
            if(side==PRO_SMT_SURF_SIDE)side_count++;
        }
    }
    if(!wall||side_count!=1)return PRO_TK_BAD_CONTEXT;
    if((error=ProSurfaceToGeomitem((ProSolid)model,wall,&item))||
       (error=fd_plane_data(&item,p,result->normal))||
       (error=ProEdgeIdGet(edge,&result->id))||
       (error=ProEdgeXyzdataEval(edge,0,result->a,d1,d2,d3))||
       (error=ProEdgeXyzdataEval(edge,1,result->b,d1,d2,d3)))return error;
    ProFeature owner;
    if((error=ProGeomitemFeatureGet(&item,&owner)))return error;
    result->face=item.id;
    result->owner=owner.id;
    result->length=cm_distance(result->a,result->b);
    ad_sub(result->b,result->a,result->axis);
    if(!ad_unit(result->axis)||!ad_unit(result->normal))return PRO_TK_BAD_CONTEXT;
    ad_cross(result->normal,result->axis,result->inward);
    if(!ad_unit(result->inward))return PRO_TK_BAD_CONTEXT;
    ProVector mid,candidate;
    cm_point(result->a,result->axis,result->length/2,mid);
    cm_point(mid,result->inward,result->length*1e-4,candidate);
    ProUvParam uv;
    ProUvStatus location;
    if((error=ProSurfaceParamEval((ProSolid)model,wall,candidate,uv))||
       (error=ProSurfaceUvpntVerify((ProSolid)model,wall,uv,&location)))return error;
    if(location!=PRO_UV_INSIDE)for(int i=0;i<3;i++)result->inward[i]*=-1;
    return PRO_TK_NO_ERROR;
}

static ProError cm_edge_get(ProMdl model,const wchar_t *token,CabinetEdge *result){
    FdRef edge={0};
    ProEdge native;
    ProError error=fd_ref_resolve(model,token,PRO_EDGE,&edge);
    if(!error)error=ProGeomitemToEdge(&edge.item,&native);
    if(!error)error=cm_edge_data(model,native,result);
    fd_ref_free(&edge);
    return error;
}

typedef struct {
    ProMdl model;
    int face_id,owner;
    const CabinetEdge *line;
    double from,to,tolerance;
    int matches;
    const double *parallel;
    CabinetEdge edge;
} CabinetEdgeSearch;

static ProError cm_edge_visit(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    CabinetEdgeSearch *search=data;
    CabinetEdge candidate={0};
    if(cm_edge_data(search->model,edge,&candidate)||candidate.face!=search->face_id)return PRO_TK_NO_ERROR;
    if(search->owner&&candidate.owner!=search->owner)return PRO_TK_NO_ERROR;
    if(search->parallel&&fabs(ad_dot(candidate.axis,search->parallel))<1-1e-8)return PRO_TK_NO_ERROR;
    if(search->line){
        const CabinetEdge *line=search->line;
        ProVector a,b;
        ad_sub(candidate.a,line->a,a);
        ad_sub(candidate.b,line->a,b);
        double t0=ad_dot(a,line->axis),t1=ad_dot(b,line->axis);
        ProVector p,q;
        cm_point(line->a,line->axis,t0,p);
        cm_point(line->a,line->axis,t1,q);
        if(cm_distance(p,candidate.a)>search->tolerance||cm_distance(q,candidate.b)>search->tolerance||
           fmin(t0,t1)>search->from+search->tolerance||fmax(t0,t1)<search->to-search->tolerance)return PRO_TK_NO_ERROR;
    }
    if(search->matches&&candidate.id==search->edge.id)return PRO_TK_NO_ERROR;
    if(!search->matches||candidate.length>search->edge.length)search->edge=candidate;
    search->matches++;
    return PRO_TK_NO_ERROR;
}

static ProError cm_edge_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    CabinetEdgeSearch *search=data;
    ProSurface surface;
    ProError error=ProSurfaceInit((ProSolid)search->model,search->face_id,&surface);
    return error?error:ProContourEdgeVisit(surface,contour,cm_edge_visit,NULL,search);
}

static ProError cm_edges_on_face(CabinetEdgeSearch *search){
    ProSurface surface;
    ProError error=ProSurfaceInit((ProSolid)search->model,search->face_id,&surface);
    if(!error)error=ProSurfaceContourVisit(surface,cm_edge_contour,NULL,search);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    return error;
}

static ProError cm_wall_face_visit(ProGeomitem *item,ProError status,ProAppData data){
    if(status)return status;
    CabinetFace *face=data;
    ProSurface surface;
    ProSmtSurfType type;
    ProVector point,normal;
    double area;
    if(ProGeomitemToSurface(item,&surface)||ProSmtSurfaceTypeGet((ProPart)item->owner,surface,&type)||
       type!=PRO_SMT_SURF_FACE||fd_plane_data(item,point,normal)||!ad_unit(normal)||
       ProSurfaceAreaEval(surface,&area)||!isfinite(area)||area<=face->area)return PRO_TK_NO_ERROR;
    face->id=item->id;
    face->area=area;
    memcpy(face->point,point,sizeof(face->point));
    memcpy(face->normal,normal,sizeof(face->normal));
    return PRO_TK_NO_ERROR;
}

static ProError cm_wall_face(ProFeature *feature,CabinetFace *face){
    memset(face,0,sizeof(*face));
    ProError error=ProFeatureGeomitemVisit(feature,PRO_SURFACE,cm_wall_face_visit,NULL,face);
    return error?error:face->id?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

typedef struct {
    ProMdl model;
    ProVector direction;
    double min,max;
    int count;
} CabinetSpan;

static ProError cm_span_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    CabinetSpan *span=data;
    ProVector point,d1,d2,d3;
    for(int i=0;i<=1;i++){
        ProError error=ProEdgeXyzdataEval(edge,i,point,d1,d2,d3);
        if(error)return error;
        double coord=ad_dot(point,span->direction);
        if(!span->count||coord<span->min)span->min=coord;
        if(!span->count||coord>span->max)span->max=coord;
        span->count++;
    }
    return PRO_TK_NO_ERROR;
}
typedef struct {ProSurface surface;CabinetSpan *span;} CabinetContour;
static ProError cm_span_contour(ProContour contour,ProError status,ProAppData data){
    if(status)return status;
    CabinetContour *bounds=data;
    return ProContourEdgeVisit(bounds->surface,contour,cm_span_edge,NULL,bounds->span);
}
static ProError cm_face_span(ProMdl model,int id,const double *direction,CabinetSpan *span){
    ProSurface surface;
    memset(span,0,sizeof(*span));
    memcpy(span->direction,direction,sizeof(span->direction));
    ProError error=ProSurfaceInit((ProSolid)model,id,&surface);
    CabinetContour bounds={surface,span};
    if(!error)error=ProSurfaceContourVisit(surface,cm_span_contour,NULL,&bounds);
    return error?error:span->count?PRO_TK_NO_ERROR:PRO_TK_E_NOT_FOUND;
}

static int cm_near(double a,double b,double scale){return fabs(a-b)<=0.0002*scale;}

static double cm_edge_height(const CabinetEdge *edge,const double *point){
    ProVector delta;
    ad_sub(point,edge->a,delta);
    return ad_dot(delta,edge->normal);
}

static ProError cm_feature_find(ProMdl model,const wchar_t *name,ProFeature *feature){
    ProFeatStatus state;
    ProError error=ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,(ProModelitem *)feature);
    if(!error)error=ProFeatureStatusGet(feature,&state);
    return error?error:state==PRO_FEAT_ACTIVE?PRO_TK_NO_ERROR:PRO_TK_BAD_CONTEXT;
}
