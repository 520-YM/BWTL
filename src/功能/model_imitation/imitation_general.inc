#include "imitation_surface_check.inc"

typedef struct {
    int quilt,face,feature;
    ProSrftype type;
    ProVector origin,normal;
    double radius,radius2;
    ProUvParam low,high;
} ImitationWallReference;

typedef struct {
    ProMdl model;
    ProOutline bounds;
    double mm, volume, area, quilt_area;
    int bodies, quilts, empty_quilts, body_shells;
    int recovered_mass,datum_faces,wall_refs,failed_quilt,failed_face;
    int quilt_ids_count,quilt_ids[128];
    const wchar_t *stage;
    ImitationWallReference wall_reference[64];
} ImitationShapeMetrics;

#include "imitation_blend_quilt.inc"
#include "imitation_quilt_check.inc"

static int imitation_metric_same(double a, double b) {
    return fabs(a - b) <= fmax(1e-6, fmax(fabs(a), fabs(b)) * 1e-5);
}

typedef struct {
    ImitationShapeMetrics *shape;
    SfArea area;
    int quilt;
} ImitationQuiltRead;

static ProError replay_symmetric_quilt_area(ProMdl model,int quilt,double *measured);

static ProError imitation_quilt_surface(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ImitationQuiltRead *read=data;
    ImitationShapeMetrics *shape=read->shape;
    ProBoolean datum=0;
    ProError error=ProSurfaceIdGet(surface,&shape->failed_face);
    if(error)return error;
    error=ProSurfaceIsDatumPlane((ProSolid)shape->model,surface,&datum);
    if(!error&&datum){shape->datum_faces++;return PRO_TK_NO_ERROR;}
    if(imitation_blend_face(shape->model,surface)){
        double area;
        error=imitation_blend_face_area(surface,shape->mm,&area);
        if(!error){read->area.area+=area;read->area.count++;}
        return error;
    }
    error=sf_surface_area(surface,PRO_TK_NO_ERROR,&read->area);
    if(error!=PRO_TK_OUT_OF_RANGE&&error!=PRO_TK_EMPTY)return error;

    /* Some wall-owned construction faces have no measurable trimmed area.
       Compare their analytic definitions separately from the solid. */
    if(shape->wall_refs==_countof(shape->wall_reference))return error;
    ProGeomitem item;
    ProFeature owner;
    ProFeattype type=0;
    ImitationWallReference *ref=&shape->wall_reference[shape->wall_refs];
    if(ProSurfaceToGeomitem((ProSolid)shape->model,surface,&item)||
       ProGeomitemFeatureGet(&item,&owner)||ProFeatureTypeGet(&owner,&type)||type!=PRO_FEAT_WALL)return error;
    ProGeomitemdata *raw=NULL;
    if(ProSurfaceDataGet(surface,&raw))return error;
    const ProSurfacedata *geometry=raw->data.p_surface_data;
    ref->type=geometry->type;
    if(ref->type==PRO_SRF_PLANE){
        memcpy(ref->origin,geometry->srf_shape.plane.origin,sizeof(ref->origin));
        memcpy(ref->normal,geometry->srf_shape.plane.e3,sizeof(ref->normal));
    }else if(ref->type==PRO_SRF_CYL){
        memcpy(ref->origin,geometry->srf_shape.cylinder.origin,sizeof(ref->origin));
        memcpy(ref->normal,geometry->srf_shape.cylinder.e3,sizeof(ref->normal));
        ref->radius=geometry->srf_shape.cylinder.radius*shape->mm;
    }
    if(ref->type==PRO_SRF_TORUS){
        memcpy(ref->origin,geometry->srf_shape.torus.origin,sizeof(ref->origin));
        memcpy(ref->normal,geometry->srf_shape.torus.e3,sizeof(ref->normal));
        ref->radius=geometry->srf_shape.torus.radius1*shape->mm;
        ref->radius2=geometry->srf_shape.torus.radius2*shape->mm;
    }
    memcpy(ref->low,geometry->uv_min,sizeof(ref->low));
    memcpy(ref->high,geometry->uv_max,sizeof(ref->high));
    ProGeomitemdataFree(&raw);
    if((ref->type!=PRO_SRF_PLANE&&ref->type!=PRO_SRF_CYL&&ref->type!=PRO_SRF_TORUS)||!ad_unit(ref->normal))return error;
    ref->quilt=read->quilt;
    ref->face=item.id;
    ref->feature=owner.id;
    for(int i=0;i<3;i++)ref->origin[i]*=shape->mm;
    shape->wall_refs++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_body_mass(ProMdl model,ProMassProperty *sum,int expected){
    ProSolidBody *bodies=NULL;
    int count=0,measured=0;
    memset(sum,0,sizeof(*sum));
    ProError error=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!error)error=ProArraySizeGet((ProArray)bodies,&count);
    for(int i=0;!error&&i<count;i++){
        ProSolidBodyState state;
        error=ProSolidBodyStateGet(&bodies[i],&state);
        if(error||state!=PRO_BODY_STATE_ACTIVE)continue;
        ProMassProperty mass;
        error=ProSolidBodyMassPropertyGet(&bodies[i],NULL,&mass);
        if(!error&&(!isfinite(mass.volume)||!isfinite(mass.surface_area)||mass.volume<=0||mass.surface_area<=0))
            error=PRO_TK_INCOMPLETE;
        if(!error){sum->volume+=mass.volume;sum->surface_area+=mass.surface_area;measured++;}
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return error?error:measured==expected?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static int imitation_wall_refs_match(const ImitationShapeMetrics *a,const ImitationShapeMetrics *b,int native_ids){
    if(a->wall_refs!=b->wall_refs)return 0;
    unsigned char used[_countof(b->wall_reference)]={0};
    for(int i=0;i<a->wall_refs;i++){
        const ImitationWallReference *old=&a->wall_reference[i];
        int found=0;
        for(int j=0;j<b->wall_refs;j++){
            const ImitationWallReference *now=&b->wall_reference[j];
            if(!used[j]&&(!native_ids||(old->quilt==now->quilt&&old->face==now->face&&old->feature==now->feature))&&
               old->type==now->type&&
               ad_dot(old->normal,now->normal)>0.999999&&ad_near(old->origin,now->origin,0.001)&&
               imitation_metric_same(old->radius,now->radius)&&imitation_metric_same(old->radius2,now->radius2)&&
               imitation_metric_same(old->low[0],now->low[0])&&imitation_metric_same(old->low[1],now->low[1])&&
               imitation_metric_same(old->high[0],now->high[0])&&imitation_metric_same(old->high[1],now->high[1])){
                used[j]=1;found=1;break;
            }
        }
        if(!found)return 0;
    }
    return 1;
}

static void imitation_shape_error(const ImitationShapeMetrics *shape,ProError error,PartText *out){
    part_text_add(out,L"几何读取停在「%ls」，Creo %d。",shape->stage,error);
    if(shape->failed_quilt)part_text_add(out,L"面组 ID %d，面 ID %d。",shape->failed_quilt,shape->failed_face);
    part_text_add(out,L"已识别 %d 个实体、%d 个可量测面组。\n",shape->bodies,shape->quilts);
}

static int imitation_body_shell(ProQuilt quilt, const SfArea *area, const ImitationShapeMetrics *shape) {
    if (shape->bodies != 1 || !imitation_metric_same(area->area * shape->mm * shape->mm, shape->area))
        return 0;
    double volume = 0;
    if (ProQuiltVolumeEval(quilt, &volume) ||
        !imitation_metric_same(fabs(volume) * shape->mm * shape->mm * shape->mm, shape->volume))
        return 0;
    ImitationBoundaryCheck check = {
        shape->model, shape->model, shape->mm, shape->mm, &shape->bounds, 0, 0, 0
    };
    return !ProQuiltSurfaceVisit(quilt, imitation_face_samples, NULL, &check) &&
           check.checked_faces == area->count && !check.unchecked_faces;
}

static ProError imitation_quilt_area(ProQuilt quilt, ProError status, ProAppData data) {
    if (status) return status;
    ImitationShapeMetrics *shape = data;
    ProBoolean internal, backup;
    ProError error = ProQuiltIsInternal(quilt, &internal);
    if (!error) error = ProQuiltIsBackupgeometry(quilt, &backup);
    if (error || internal || backup) return error;

    ImitationQuiltRead read={0};
    read.shape=shape;
    error=ProQuiltIdGet(quilt,&read.quilt);
    if(error)return error;
    shape->failed_quilt=read.quilt;
    shape->failed_face=0;
    int previous=shape->wall_refs;
    error = ProQuiltSurfaceVisit(quilt, imitation_quilt_surface, NULL, &read);
    if(error==PRO_TK_OUT_OF_RANGE&&read.area.count==0&&previous==shape->wall_refs){
        double recovered=0;
        ProError fallback=replay_symmetric_quilt_area(shape->model,read.quilt,&recovered);
        trace("Recover symmetric thickening source quilt",fallback);
        if(!fallback){
            shape->quilts++;
            shape->quilt_area+=recovered*shape->mm*shape->mm;
            shape->failed_quilt=shape->failed_face=0;
            return PRO_TK_NO_ERROR;
        }
    }
    if (error && error != PRO_TK_E_NOT_FOUND) return error;
    shape->failed_quilt=shape->failed_face=0;
    if (!read.area.count) {
        if(previous==shape->wall_refs)shape->empty_quilts++;
        return PRO_TK_NO_ERROR;
    }
    if (previous==shape->wall_refs&&imitation_body_shell(quilt, &read.area, shape)) {
        shape->body_shells++;
        return PRO_TK_NO_ERROR;
    }
    shape->quilts++;
    if(shape->quilt_ids_count==_countof(shape->quilt_ids))return PRO_TK_OUT_OF_RANGE;
    shape->quilt_ids[shape->quilt_ids_count++]=read.quilt;
    shape->quilt_area += read.area.area * shape->mm * shape->mm;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_material_outline(ProMdl model,ProOutline bounds){
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProSolidOutlExclTypes excludes[]={PRO_OUTL_EXC_DATUM_PLANE,PRO_OUTL_EXC_DATUM_POINT,
        PRO_OUTL_EXC_DATUM_CSYS,PRO_OUTL_EXC_DATUM_AXES,PRO_OUTL_EXC_FACETS,PRO_OUTL_EXC_ALL_CRVS};
    return ProSolidOutlineCompute((ProSolid)model,identity,excludes,_countof(excludes),bounds);
}


static ProError imitation_shape_read(ProMdl model, ImitationShapeMetrics *shape) {
    memset(shape, 0, sizeof(*shape));
    shape->stage=L"模型单位";
    ProError error = qa_mm_per_unit(model, &shape->mm);
    shape->model = model;
    if (error) return error;

    shape->stage=L"实体列表";
    error = sd_body_count(model, &shape->bodies);
    if (error) return error;
    if (shape->bodies) {
        shape->stage=L"实体体积和表面积";
        ProMassProperty mass;
        error = qa_geometry_mass(model, &mass);
        if(error||!isfinite(mass.surface_area)||mass.surface_area<=0){
            error=imitation_body_mass(model,&mass,shape->bodies);
            if(error)return error;
            shape->recovered_mass=1;
        }
        shape->volume = mass.volume;
        shape->area = mass.surface_area;
    }
    shape->volume *= shape->mm * shape->mm * shape->mm;
    shape->area *= shape->mm * shape->mm;
    shape->stage=L"面组与面面积";
    error = ProSolidQuiltVisit((ProSolid)model, imitation_quilt_area, NULL, shape);
    if (error && error != PRO_TK_E_NOT_FOUND) return error;
    shape->stage=L"外形包络";
    error=shape->bodies&&!shape->quilts?imitation_material_outline(model,shape->bounds):
        ProSolidOutlineGet((ProSolid)model,shape->bounds);
    if(error)return error;
    for(int i=0;i<2;i++)for(int j=0;j<3;j++){
        shape->bounds[i][j]*=shape->mm;
        if(!isfinite(shape->bounds[i][j]))return PRO_TK_INCOMPLETE;
    }
    shape->stage=L"有效实体和面组";
    return shape->bodies || shape->quilts ? PRO_TK_NO_ERROR : PRO_TK_EMPTY;
}

static void imitation_report_metrics(const ImitationShapeMetrics *source,const ImitationShapeMetrics *result,
                                     PartText *out){
    part_text_add(out,L"实体 %d → %d（差 %+d），可量测面组 %d → %d（差 %+d）。\n",
        source->bodies,result->bodies,result->bodies-source->bodies,
        source->quilts,result->quilts,result->quilts-source->quilts);
    if(source->bodies&&result->bodies){
        const wchar_t *axis[]={L"X",L"Y",L"Z"};
        for(int j=0;j<3;j++){
            double a=source->bounds[1][j]-source->bounds[0][j];
            double b=result->bounds[1][j]-result->bounds[0][j];
            part_text_add(out,L"%ls 向包络 %.9g → %.9g mm（尺寸差 %+.9g，最小端偏移 %+.9g，最大端偏移 %+.9g）。\n",
                axis[j],a,b,b-a,result->bounds[0][j]-source->bounds[0][j],result->bounds[1][j]-source->bounds[1][j]);
        }
    }
    part_text_add(out,L"实体表面积 %.10g → %.10g mm²（差 %+.9g），实体体积 %.10g → %.10g mm³（差 %+.9g）。\n",
        source->area,result->area,result->area-source->area,
        source->volume,result->volume,result->volume-source->volume);
    if(source->quilts||result->quilts)
        part_text_add(out,L"面组面积 %.10g → %.10g mm²（差 %+.9g）。\n",
            source->quilt_area,result->quilt_area,result->quilt_area-source->quilt_area);
}

static ProError imitation_shape_verify_impl(ProMdl source,ProMdl result,const ImitationShapeMetrics *a,
                                        int native_ids,int report_metrics,PartText *out){
    ImitationShapeMetrics b;
    ProError error = imitation_shape_read(result, &b);
    if (error){imitation_shape_error(&b,error,out);return error;}
    if(report_metrics)imitation_report_metrics(a,&b,out);
    if(!imitation_wall_refs_match(a,&b,native_ids)){
        part_text_add(out,L"钣金壁定位面定义不同，原件 %d 张，结果 %d 张。\n",a->wall_refs,b.wall_refs);
        return PRO_TK_INCOMPLETE;
    }
    if(a->wall_refs)part_text_add(out,L"%d 张钣金壁构造面无可用面积，已逐项核对归属、平面、圆柱或环面定义及参数范围。\n",a->wall_refs);
    const double tol = 0.001;
    double bounds_error = 0;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            bounds_error = fmax(bounds_error, fabs(a->bounds[i][j] - b.bounds[i][j]));

    const int solid_diff=a->bodies!=b.bodies||bounds_error>tol||
        !imitation_metric_same(a->area,b.area)||!imitation_metric_same(a->volume,b.volume);
    if(solid_diff){
        part_text_add(out, L"实体校验差异。实体 %d/%d，包络偏差 %.6g mm，实体面积 %.10g / %.10g mm²，体积 %.10g / %.10g mm³。\n",
                      a->bodies, b.bodies, bounds_error, a->area, b.area, a->volume, b.volume);
        if(!report_metrics||!a->bodies||!b.bodies)return PRO_TK_INCOMPLETE;
    }
    if (a->quilts != b.quilts || !imitation_metric_same(a->quilt_area, b.quilt_area)) {
        part_text_add(out, L"独立面组校验差异。原件 %d 个、%.10g mm²，结果 %d 个、%.10g mm²。\n",
                      a->quilts, a->quilt_area, b.quilts, b.quilt_area);
        return PRO_TK_INCOMPLETE;
    }
    if (a->empty_quilts || b.empty_quilts || a->body_shells || b.body_shells)
        part_text_add(out, L"空面组及已核实与实体重合的外壳已单独识别。原件空面组 %d、实体外壳 %d，结果空面组 %d、实体外壳 %d。\n",
                      a->empty_quilts, a->body_shells, b.empty_quilts, b.body_shells);
    if (a->bodies) {
        QualityComparison check;
        ProVector lower, upper;
        memcpy(lower, a->bounds[0], sizeof(lower));
        memcpy(upper, a->bounds[1], sizeof(upper));
        error = qa_compare_rays_with_check(source, result, a->mm, b.mm, lower, upper, NULL, 0, 11, tol, &check,imitation_ray_crossing_match);
        if(!error&&!check.different&&check.checked<300)
            error=qa_compare_rays_with_check(source,result,a->mm,b.mm,lower,upper,NULL,0,15,tol,&check,imitation_ray_crossing_match);
        if (error) return error;
        const int ray_diff=check.different||!check.material_rays||check.checked<300;
        if(ray_diff){
            part_text_add(out, L"实体截面校验未通过，有效 %d，穿过实体 %d，差异 %d。\n",
                          check.checked, check.material_rays, check.different);
            if(!report_metrics)return PRO_TK_INCOMPLETE;
        }
        ImitationBoundaryCheck forward={0},reverse={0};
        error = imitation_check_body_faces(source, result, a->mm, b.mm, &b.bounds,report_metrics,&forward);
        ProError back=PRO_TK_NO_ERROR;
        if(!error||(report_metrics&&error==PRO_TK_INCOMPLETE))
            back=imitation_check_body_faces(result,source,b.mm,a->mm,&a->bounds,report_metrics,&reverse);
        imitation_face_difference(&forward,L"原件",out);
        imitation_face_difference(&reverse,L"结果",out);
        if(!error)error=back;
        if (error) {
            part_text_add(out, L"局部实体表面位置未通过核对。\n");
            return error;
        }
        if(solid_diff||ray_diff)return PRO_TK_INCOMPLETE;
        part_text_add(out, L"%d 个实体，包络、体积、表面积与 %d 条实体截面采样通过。局部表面双向核对 %d/%d 个，其中圆柱面 %d/%d 个。\n",
                      b.bodies, check.material_rays, forward.checked_faces, reverse.checked_faces,
                      forward.cylindrical_faces, reverse.cylindrical_faces);
        if (forward.unchecked_faces || reverse.unchecked_faces)
            part_text_add(out, L"原件 %d 个面和结果 %d 个面未取得有效局部采样，已执行整体实体校验。\n",
                          forward.unchecked_faces, reverse.unchecked_faces);
    }
    if (a->quilts){
        error=imitation_quilt_compare(a,&b,tol,out);
        if(error)return error;
        part_text_add(out, L"%d 个面组的数量、包络与总面积已核对。\n", b.quilts);
    }
    return PRO_TK_NO_ERROR;
}

static ProError imitation_shape_verify_mode(ProMdl source,ProMdl result,const ImitationShapeMetrics *a,int native_ids,PartText *out){
    return imitation_shape_verify_impl(source,result,a,native_ids,0,out);
}

static ProError imitation_shape_verify(ProMdl source,ProMdl result,const ImitationShapeMetrics *a,PartText *out){
    return imitation_shape_verify_mode(source,result,a,1,out);
}

typedef struct {
    ProMdl result;
    int features, dimensions;
    PartText *out;
} ImitationNativeCheck;

typedef struct {int ids[2048],count;} ImitationFeatureOrder;

static ProError imitation_collect_order(ProFeature *feature,ProError status,ProAppData data){
    ImitationFeatureOrder *order=data;
    if(status)return status;
    if(order->count==_countof(order->ids))return PRO_TK_OUT_OF_RANGE;
    order->ids[order->count++]=feature->id;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_check_order(ProMdl source,ProMdl target,PartText *out){
    ImitationFeatureOrder a={0},b={0};
    ProError error=ProSolidFeatVisit((ProSolid)source,imitation_collect_order,NULL,&a);
    if(!error)error=ProSolidFeatVisit((ProSolid)target,imitation_collect_order,NULL,&b);
    if(error)return error;
    int i=0;
    while(i<a.count&&i<b.count&&a.ids[i]==b.ids[i])i++;
    if(i==a.count&&i==b.count)return PRO_TK_NO_ERROR;
    part_text_add(out,L"原生特征遍历顺序从第 %d 项起有差异，特征 ID %d / %d。\n",
        i+1,i<a.count?a.ids[i]:0,i<b.count?b.ids[i]:0);
    return PRO_TK_INCOMPLETE;
}

static ProError imitation_check_feature(ProFeature *feature, ProError status, ProAppData data) {
    if (status) return status;
    ImitationNativeCheck *check = data;
    ProFeature target;
    ProFeattype a, b;
    ProFeatStatus sa, sb;
    ProError error = ProFeatureInit((ProSolid)check->result, feature->id, &target);
    if (!error) error = ProFeatureTypeGet(feature, &a);
    if (!error) error = ProFeatureTypeGet(&target, &b);
    if (!error) error = ProFeatureStatusGet(feature, &sa);
    if (!error) error = ProFeatureStatusGet(&target, &sb);
    if (error) return error;
    ProBoolean embedded_a=PRO_B_FALSE,embedded_b=PRO_B_FALSE;
    ProError embedded_status_a=ProFeatureIsEmbedded(feature,&embedded_a);
    ProError embedded_status_b=ProFeatureIsEmbedded(&target,&embedded_b);
    if (a != b || sa != sb || embedded_status_a!=embedded_status_b || embedded_a!=embedded_b){
        if(check->out)part_text_add(check->out,L"特征 ID %d，类型 %d/%d，状态 %d/%d。\n",
            feature->id,a,b,sa,sb);
        return PRO_TK_INCOMPLETE;
    }
    check->features++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_check_dimension(ProDimension *dimension, ProError status, ProAppData data) {
    if (status) return status;
    double a, b;
    if (ProDimensionValueGet(dimension, &a)) return PRO_TK_NO_ERROR;
    ImitationNativeCheck *check = data;
    ProDimension target;
    ProError error = ProModelitemInit(check->result, dimension->id, PRO_DIMENSION, &target);
    if (!error) error = ProDimensionValueGet(&target, &b);
    if (error) return error;
    if (!part_close(a, b)){
        if(check->out)part_text_add(check->out,L"尺寸 ID %d，原件 %.10g，结果 %.10g（模型单位）。\n",dimension->id,a,b);
        return PRO_TK_INCOMPLETE;
    }
    check->dimensions++;
    return PRO_TK_NO_ERROR;
}

static ProError imitation_native_new(ProMdl source, ProMdlName name, ProMdlsubtype subtype,
                                     ProMdl *result, PartText *out) {
    ProError error = ProMdlnameCopy(source, name, result);
    if (!error) error = part_regenerate(*result);
    ProMdlsubtype actual;
    if (!error) error = ProMdlSubtypeGet(*result, &actual);
    if (!error && actual != subtype) error = PRO_TK_INCOMPLETE;
    ImitationNativeCheck check = {*result, 0, 0, out};
    if (!error) error = ProSolidFeatVisit((ProSolid)source, imitation_check_feature, NULL, &check);
    if (!error) error = imitation_check_order(source,*result,out);
    if (!error) {
        error = ProSolidDimensionVisit((ProSolid)source, PRO_B_FALSE, imitation_check_dimension, NULL, &check);
        if (error == PRO_TK_E_NOT_FOUND && !check.dimensions) error = PRO_TK_NO_ERROR;
    }
    if (!error) part_text_add(out, L"原生特征保留。Creo 生成了独立的可编辑副本，并已重生成，核对 %d 项特征和 %d 个驱动尺寸。\n",
                              check.features, check.dimensions);
    return error;
}

static ProError imitation_step_new(ProMdl source, const DeliveryOutput *work, ProMdlName name, ProMdl *result) {
    ProPath base, path;
    ProError error = fab_leaf_path(work->stage, L"geometry", L"", base);
    if (!error) error = fab_leaf_path(work->stage, L"geometry", L".stp", path);
    if (!error) error = ProIntf3DFileWriteWithDefaultProfile((ProSolid)source, PRO_INTF_EXPORT_STEP, base);
    if (error) return error;
    if (!dv_file_nonempty(path)) error = fab_leaf_path(work->stage, L"geometry", L".step", path);
    if (!error) error = dv_source_part(path, PRO_INTF_IMPORT_STEP);
    if (!error) error = ProIntfimportModelWithOptionsMdlnameCreate(path, NULL, PRO_INTF_IMPORT_STEP,
        PRO_MDL_PART, PRO_IMPORTREP_MASTER, name, NULL, NULL, result);
    if (!error) error = part_health(*result);
    return error;
}

