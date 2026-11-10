#define DV_BORE_MAX 128
#define DV_BORE_SURFACES 24
typedef struct {
    ProVector centerline,axis;
    double radius,low,high;
    int faces[DV_BORE_SURFACES],face_count;
    int classification;
} DeliveryBore;

typedef struct {
    ProMdl model;
    int owner;
    double tolerance;
    int count,limited;
    DeliveryBore bores[DV_BORE_MAX];
} DeliveryBores;

static ProError dv_collect_bore(ProSurface surface,ProError status,ProAppData context){
    DeliveryBores *scan=context;
    if(status)return status;
    ProSrftype type;
    ProGeomitem item;
    ProFeature owner;
    ProError error;
    if((error=ProSurfaceTypeGet(surface,&type)))return error;
    if(type!=PRO_SRF_CYL)return PRO_TK_NO_ERROR;
    if((error=ProSurfaceToGeomitem((ProSolid)scan->model,surface,&item))||
       (error=ProGeomitemFeatureGet(&item,&owner)))return error;
    if(scan->owner&&owner.id!=scan->owner)return PRO_TK_NO_ERROR;
    wchar_t token[32];
    swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
    QualityBore bore;
    FdRef ref={0};
    error=qa_bore(scan->model,token,&ref,&bore);
    fd_ref_free(&ref);
    if(error==PRO_TK_BAD_CONTEXT||error==PRO_TK_INCOMPLETE)return PRO_TK_NO_ERROR;
    if(error)return error;
    int dominant=0;
    for(int i=1;i<3;i++)if(fabs(bore.axis[i])>fabs(bore.axis[dominant]))dominant=i;
    if(bore.axis[dominant]<0)for(int i=0;i<3;i++)bore.axis[i]=-bore.axis[i];

    ProVector low_point,high_point,centerline;
    if((error=ProSurfaceExtremesEval(surface,bore.axis,low_point,high_point)))return error;
    double low=ad_dot(low_point,bore.axis),high=ad_dot(high_point,bore.axis);
    if(high-low<=scan->tolerance)return PRO_TK_NO_ERROR;
    double axial=ad_dot(bore.origin,bore.axis);
    for(int i=0;i<3;i++)centerline[i]=bore.origin[i]-axial*bore.axis[i];

    DeliveryBore *entry=NULL;
    for(int i=0;i<scan->count;i++){
        DeliveryBore *candidate=&scan->bores[i];
        if(fabs(bore.radius-candidate->radius)<=scan->tolerance&&
           ad_dot(candidate->axis,bore.axis)>1.0-1e-8&&
           ad_near(candidate->centerline,centerline,scan->tolerance)&&
           low<=candidate->high+scan->tolerance&&high>=candidate->low-scan->tolerance){
            entry=candidate;
            break;
        }
    }
    if(!entry){
        if(scan->count==DV_BORE_MAX){scan->limited=1;return PRO_TK_NO_ERROR;}
        entry=&scan->bores[scan->count++];
        memcpy(entry->centerline,centerline,sizeof(centerline));
        memcpy(entry->axis,bore.axis,sizeof(bore.axis));
        entry->radius=bore.radius;
        entry->low=low;
        entry->high=high;
    }
    if(entry->face_count==DV_BORE_SURFACES){scan->limited=1;return PRO_TK_NO_ERROR;}
    entry->faces[entry->face_count++]=item.id;
    entry->low=fmin(entry->low,low);
    entry->high=fmax(entry->high,high);
    return PRO_TK_NO_ERROR;
}

static void dv_bore_classify(ProMdl model,DeliveryBore *bore,double epsilon){
    ProSurface faces[DV_BORE_SURFACES];
    ProVector seed={1,0,0},u,v;
    double mid=(bore->low+bore->high)/2.0;
    if(fabs(bore->axis[0])>0.8){seed[0]=0;seed[1]=1;}
    ad_cross(seed,bore->axis,u);
    if(!ad_unit(u))return;
    ad_cross(bore->axis,u,v);
    for(int i=0;i<bore->face_count;i++){
        ProGeomitem item;
        if(ProModelitemInit(model,bore->faces[i],PRO_SURFACE,&item)||ProGeomitemToSurface(&item,&faces[i]))return;
    }
    for(int i=0;i<24;i++){
        ProVector point;
        double angle=(i+0.37)*2.0*AS_PI/24.0;
        for(int j=0;j<3;j++)
            point[j]=bore->centerline[j]+mid*bore->axis[j]+bore->radius*(cos(angle)*u[j]+sin(angle)*v[j]);
        int covered=0;
        for(int j=0;j<bore->face_count&&!covered;j++){
            ProUvParam uv;
            ProUvStatus status;
            if(!ProSurfaceParamEval((ProSolid)model,faces[j],point,uv)&&
               !ProSurfaceUvpntVerify((ProSolid)model,faces[j],uv,&status)&&status==PRO_UV_INSIDE)covered=1;
        }
        if(!covered)return;
    }

    bore->classification=1;
    ProRay ray;
    for(int i=0;i<3;i++)ray.start_point[i]=bore->centerline[i]+mid*bore->axis[i];
    memcpy(ray.dir_vector,bore->axis,sizeof(bore->axis));
    QualityRay hits;
    if(qa_ray(model,&ray,epsilon,&hits))return;
    if(hits.count==0){bore->classification=2;return;}
    QualityCrossing first=hits.crossing[0],last=hits.crossing[hits.count-1];
    int cap=0;
    if(first.depth>0&&fabs(first.depth-(bore->high-mid))<=epsilon*10.0)cap=first.surface_id;
    if(last.depth<0&&fabs(last.depth-(bore->low-mid))<=epsilon*10.0)cap=last.surface_id;
    ProModelitem item;
    ProVector point,normal;
    if(cap&&!ProModelitemInit(model,cap,PRO_SURFACE,&item)&&!fd_plane_data(&item,point,normal)&&
       ad_unit(normal)&&fabs(ad_dot(normal,bore->axis))>1-1e-8)bore->classification=3;
}

static ProError dv_bores_scan(ProMdl model,int owner,double scale,DeliveryBores *scan){
    ProError error=qa_single_body(model,NULL);
    if(error)return error;
    scan->model=model;
    scan->owner=owner;
    scan->tolerance=0.00001/scale;
    error=ProSolidSurfaceVisit((ProSolid)model,dv_collect_bore,NULL,scan);
    if(error==PRO_TK_E_NOT_FOUND)error=PRO_TK_NO_ERROR;
    if(error)return error;
    for(int i=0;i<scan->count;i++)dv_bore_classify(model,&scan->bores[i],scan->tolerance);
    return PRO_TK_NO_ERROR;
}

static const wchar_t *dv_bore_class(int classification){
    return classification==2?L"通孔候选":classification==3?L"平底盲孔候选":classification==1?L"圆柱孔候选":L"局部圆柱内凹面";
}

static ProError dv_recognize(ProMdl model,const PartCommand *cmd,PartText *out){
    ProFeature feature;
    ProFeattype type;
    double scale;
    ProError error;
    int feature_id;
    if(qa_ref(cmd->arg[2],L"ID:")){
        part_integer(cmd->arg[2]+3,&feature_id);
        error=ProFeatureInit((ProSolid)model,feature_id,&feature);
    }else error=ux_named_feature(model,cmd->arg[2],&feature);
    if(error||(error=ProFeatureTypeGet(&feature,&type))||(error=qa_mm_per_unit(model,&scale)))return error;
    if(type!=PRO_FEAT_IMPORT)return PRO_TK_BAD_CONTEXT;
    DeliveryBores *scan=calloc(1,sizeof(*scan));
    if(!scan)return PRO_TK_OUT_OF_MEMORY;
    error=dv_bores_scan(model,feature.id,scale,scan);
    if(!error){
        part_text_add(out,L"导入特征 %ls（ID %d）识别到 %d 组内凹圆柱面。\n",cmd->arg[2],feature.id,scan->count);
        for(int i=0;i<scan->count;i++){
            DeliveryBore *bore=&scan->bores[i];
            part_text_add(out,L"%ls，SURFACE:%d，直径 %.9g mm，轴向跨度 %.9g mm，轴向 (%.6g, %.6g, %.6g)，关联面 %d。\n",
                dv_bore_class(bore->classification),bore->faces[0],2*bore->radius*scale,
                (bore->high-bore->low)*scale,bore->axis[0],bore->axis[1],bore->axis[2],bore->face_count);
        }
        if(scan->limited)part_text_add(out,L"达到扫描上限，结果覆盖部分候选面。\n");
    }
    free(scan);
    return error;
}
