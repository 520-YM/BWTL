typedef struct {
    ProSrftype kind;
    int count;
    ProVector point[9],normal[9];
} ReplayTrimmedFace;

static ProError replay_trimmed_face_read(ProSurface surface,ProSolid owner,ReplayTrimmedFace *face){
    ProGeomitemdata *raw=NULL;
    ProError error=ProSurfaceDataGet(surface,&raw);
    if(error)return error;
    const ProSurfacedata *data=raw->data.p_surface_data;
    face->kind=data->type;
    const double positions[]={0.5,0.23,0.77};
    for(int i=0;!error&&i<3;i++)for(int j=0;!error&&j<3;j++){
        ProUvParam uv={
            data->uv_min[0]+positions[i]*(data->uv_max[0]-data->uv_min[0]),
            data->uv_min[1]+positions[j]*(data->uv_max[1]-data->uv_min[1])
        };
        ProUvStatus location;
        error=ProSurfaceUvpntVerify(owner,surface,uv,&location);
        if(error||location!=PRO_UV_INSIDE)continue;
        const int next=face->count;
        error=ProSurfaceXyzdataEval(surface,uv,face->point[next],NULL,NULL,face->normal[next]);
        if(!error&&!ad_unit(face->normal[next]))error=PRO_TK_INCOMPLETE;
        if(!error)face->count++;
    }
    ProGeomitemdataFree(&raw);
    return error?error:face->count>=3?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}


static int replay_trimmed_sample(ProSurface face,ProSolid owner,const double *position,const double *direction){
    ProUvParam uv;
    ProUvStatus location;
    ProVector point,normal,input;
    memcpy(input,position,sizeof(input));
    return !ProSurfaceParamEval(owner,face,input,uv)&&!ProSurfaceUvpntVerify(owner,face,uv,&location)&&
        location==PRO_UV_INSIDE&&!ProSurfaceXyzdataEval(face,uv,point,NULL,NULL,normal)&&
        ad_near(point,position,0.0002)&&ad_unit(normal)&&ad_dot(direction,normal)>0.999999;
}

static int replay_trimmed_face_matches(ProGeomitem *item,ProGeomitem *original,const ReplayTrimmedFace *source){
    ProSurface surface;
    ProSrftype kind;
    if(ProGeomitemToSurface(item,&surface)||ProSurfaceTypeGet(surface,&kind)||kind!=source->kind)return 0;
    ReplayTrimmedFace candidate={0};
    if(replay_trimmed_face_read(surface,(ProSolid)item->owner,&candidate))return 0;
    for(int i=0;i<source->count;i++)if(!replay_trimmed_sample(surface,(ProSolid)item->owner,
       source->point[i],source->normal[i]))return 0;
    ProSurface source_surface;
    if(ProGeomitemToSurface(original,&source_surface))return 0;
    for(int i=0;i<candidate.count;i++)if(!replay_trimmed_sample(source_surface,(ProSolid)original->owner,
       candidate.point[i],candidate.normal[i]))return 0;
    return 1;
}

typedef struct {ProEnttype kind; double length; ProVector point[9];} ReplayCurve;

static ProError replay_curve_read(ProGeomitem *item,ReplayCurve *shape){
    ProCurve curve;
    ProError error=ProGeomitemToCurve(item,&curve);
    if(!error)error=ProCurveTypeGet(curve,&shape->kind);
    if(!error)error=ProCurveLengthEval(curve,&shape->length);
    for(int i=0;!error&&i<_countof(shape->point);i++)
        error=ProCurveXyzdataEval(curve,i/8.0,shape->point[i],NULL,NULL);
    return error;
}

static int replay_curve_same(const ReplayCurve *a,const ReplayCurve *b){
    if(a->kind!=b->kind||fabs(a->length-b->length)>0.0002)return 0;
    for(int i=0;i<_countof(a->point);i++)if(!ad_near(a->point[i],b->point[i],0.0002))return 0;
    return 1;
}

static ProError replay_verify_equation_curve(ProFeature *source,ProFeature *target){
    ProGeomitem a,b;
    ProCurve curve;
    ReplayCurve original={0},rebuilt={0};
    ProError error=ad_single_curve(source,&a,&curve);
    if(!error)error=ad_single_curve(target,&b,&curve);
    if(!error)error=replay_curve_read(&a,&original);
    if(!error)error=replay_curve_read(&b,&rebuilt);
    if(!error&&!replay_curve_same(&original,&rebuilt))error=PRO_TK_INCOMPLETE;
    return error;
}

