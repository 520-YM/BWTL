#define QA_MAX_CROSSINGS 128
typedef struct {
    double depth;
    int surface_id;
    int entering;
} QualityCrossing;
typedef struct {
    QualityCrossing crossing[QA_MAX_CROSSINGS];
    int count;
} QualityRay;

static int qa_crossing_order(const void *first,const void *second){
    const QualityCrossing *a=first,*b=second;
    return (a->depth>b->depth)-(a->depth<b->depth);
}

static ProError qa_ray(ProMdl model,ProRay *line,double epsilon,QualityRay *result){
    /* The Toolkit owns and reuses its returned selection array. */
    ProSelection *hits=NULL;
    int count=0;
    result->count=0;
    ProError error=ProSolidRayIntersectionCompute(model,0.1,line,&hits,&count);
    if(error==PRO_TK_E_NOT_FOUND)return PRO_TK_NO_ERROR;
    if(error)return error;
    for(int i=0;i<count;i++){
        ProModelitem item;
        ProSurface surface;
        ProBoolean datum;
        ProQuilt quilt;
        ProUvParam uv;
        ProVector point,normal;
        double depth;
        if((error=ProSelectionModelitemGet(hits[i],&item)))return error;
        if(item.type!=PRO_SURFACE)continue;
        if((error=ProGeomitemToSurface(&item,&surface))||
           (error=ProSurfaceIsDatumPlane((ProSolid)model,surface,&datum)))return error;
        if(datum)continue;
        error=ProSurfaceQuiltGet((ProSolid)model,surface,&quilt);
        if(!error)continue;
        if(error!=PRO_TK_E_NOT_FOUND)return error;
        if((error=ProSelectionDepthGet(hits[i],&depth))||
           (error=ProSelectionUvParamGet(hits[i],uv))||
           (error=ProSurfaceXyzdataEval(surface,uv,point,NULL,NULL,normal)))return error;
        if(!isfinite(depth)||!ad_unit(normal))return PRO_TK_INCOMPLETE;
        double alignment=ad_dot(normal,line->dir_vector);
        if(fabs(alignment)<1e-8)return PRO_TK_INCOMPLETE;
        if(result->count==QA_MAX_CROSSINGS)return PRO_TK_INCOMPLETE;
        QualityCrossing crossing={depth,item.id,alignment<0};
        result->crossing[result->count++]=crossing;
    }
    qsort(result->crossing,result->count,sizeof(QualityCrossing),qa_crossing_order);
    int unique=0;
    for(int i=0;i<result->count;i++){
        QualityCrossing hit=result->crossing[i];
        if(unique&&fabs(hit.depth-result->crossing[unique-1].depth)<=epsilon){
            if(hit.entering!=result->crossing[unique-1].entering)return PRO_TK_INCOMPLETE;
            continue;
        }
        result->crossing[unique++]=hit;
    }
    if(unique%2)return PRO_TK_INCOMPLETE;
    for(int i=0;i<unique;i++)
        if(result->crossing[i].entering!=(i%2==0))return PRO_TK_INCOMPLETE;
    result->count=unique;
    return PRO_TK_NO_ERROR;
}
