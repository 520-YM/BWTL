/* Geometry at a trimmed face point is checked against solid ray hits.
   Work in mm across the source and destination unit systems. */
static ProError imitation_boundary_at(ProMdl model, double mm, const ProOutline bounds,
                                      const ProVector position, const ProVector normal, int *matched) {
    *matched = 0;
    double distance = cm_distance(bounds[0], bounds[1]) + 2.0;
    ProRay ray = {0};
    for (int i = 0; i < 3; i++) {
        ray.dir_vector[i] = normal[i];
        ray.start_point[i] = (position[i] - distance * normal[i]) / mm;
    }
    ProSelection *hits = NULL;
    int count = 0;
    ProError error = ProSolidRayIntersectionCompute((ProSolid)model, 0.1, &ray, &hits, &count);
    if (error == PRO_TK_E_NOT_FOUND) return PRO_TK_NO_ERROR;
    if (error) return error;
    for (int i = 0; i < count; i++) {
        double depth;
        ProModelitem item;
        if ((error = ProSelectionDepthGet(hits[i], &depth))) return error;
        if (fabs(depth * mm - distance) > 0.001) continue;
        if ((error = ProSelectionModelitemGet(hits[i], &item))) return error;
        if (item.type != PRO_SURFACE) continue;
        ProSurface surface;
        ProBoolean datum;
        if ((error = ProGeomitemToSurface(&item, &surface)) ||
            (error = ProSurfaceIsDatumPlane((ProSolid)model, surface, &datum))) return error;
        if (datum) continue;
        ProQuilt quilt;
        error = ProSurfaceQuiltGet((ProSolid)model, surface, &quilt);
        if (!error) continue;
        if (error != PRO_TK_E_NOT_FOUND) return error;
        ProUvParam uv;
        ProVector point, hit_normal;
        if ((error = ProSelectionUvParamGet(hits[i], uv)) ||
            (error = ProSurfaceXyzdataEval(surface, uv, point, NULL, NULL, hit_normal))) return error;
        if (ad_unit(hit_normal) && fabs(ad_dot(hit_normal, normal)) > 0.9999) {
            *matched = 1;
            return PRO_TK_NO_ERROR;
        }
    }
    return PRO_TK_NO_ERROR;
}

typedef struct {
    ProMdl source, target;
    double source_mm, target_mm;
    const ProOutline *target_bounds;
    int checked_faces, unchecked_faces, cylindrical_faces;
    int report_local;
    int different_face;
    ProSrftype different_type;
    ProVector different_point;
    double different_radius;
    ProVector different_axis;
    int differences;
} ImitationBoundaryCheck;

static ProError imitation_face_samples(ProSurface face, ProError status, ProAppData data) {
    if (status) return status;
    ImitationBoundaryCheck *check = data;
    ProGeomitemdata *raw = NULL;
    ProError error = ProSurfaceDataGet(face, &raw);
    if (error) return error;
    const ProSurfacedata *surface = raw->data.p_surface_data;
    /* Sample near boundaries as well as in the middle of the UV range.
       Thin frame walls may have no material in the middle. */
    static const double fractions[] = {0.50, 0.04, 0.96, 0.22, 0.78, 0.008, 0.992};
    int sampled = 0;
    int n = (int)_countof(fractions);
    for (int offset = 0; !error && offset < n && sampled < 3; offset++) {
        for (int row = 0; !error && row < n && sampled < 3; row++) {
            int col = (row + offset) % n;
            ProUvParam uv = {
                surface->uv_min[0] + fractions[row] * (surface->uv_max[0] - surface->uv_min[0]),
                surface->uv_min[1] + fractions[col] * (surface->uv_max[1] - surface->uv_min[1])
            };
            ProUvStatus inside;
            error = ProSurfaceUvpntVerify((ProSolid)check->source, face, uv, &inside);
            if (error || inside != PRO_UV_INSIDE) continue;
            ProVector point, normal;
            error = ProSurfaceXyzdataEval(face, uv, point, NULL, NULL, normal);
            if (error || !ad_unit(normal)) continue;
            for (int i = 0; i < 3; i++) point[i] *= check->source_mm;
            int matched;
            error = imitation_boundary_at(check->target, check->target_mm, *check->target_bounds,
                                          point, normal, &matched);
            if (!error && !matched) {
                check->differences++;
                if(!check->different_face||(check->different_type!=PRO_SRF_CYL&&surface->type==PRO_SRF_CYL)){
                    ProSurfaceIdGet(face,&check->different_face);
                    check->different_type=surface->type;
                    memcpy(check->different_point,point,sizeof(point));
                    if(surface->type==PRO_SRF_CYL){
                        const ProCylinderdata *cylinder=&surface->srf_shape.cylinder;
                        check->different_radius=fabs(cylinder->radius)*check->source_mm;
                        const double along=ad_dot(cylinder->origin,cylinder->e3);
                        for(int i=0;i<3;i++)
                            check->different_axis[i]=(cylinder->origin[i]-along*cylinder->e3[i])*check->source_mm;
                    }
                }
                error = PRO_TK_INCOMPLETE;
            }
            sampled++;
        }
    }
    if (!error && sampled) {
        check->checked_faces++;
        if (surface->type == PRO_SRF_CYL) check->cylindrical_faces++;
    }
    if (!error && !sampled) check->unchecked_faces++;
    ProGeomitemdataFree(&raw);
    return check->report_local&&error==PRO_TK_INCOMPLETE?PRO_TK_NO_ERROR:error;
}

static void imitation_face_difference(const ImitationBoundaryCheck *check,const wchar_t *side,PartText *out)
{
    if(!check->different_face)return;
    const double *p=check->different_point;
    part_text_add(out,L"%ls SURFACE:%d 在 (%.6g, %.6g, %.6g) mm 附近有局部差异",
        side,check->different_face,p[0],p[1],p[2]);
    if(check->different_type==PRO_SRF_CYL){
        const double *axis=check->different_axis;
        part_text_add(out,L"，圆柱面直径 %.6g mm，轴线距模型原点最近点 (%.6g, %.6g, %.6g) mm",
            2*check->different_radius,axis[0],axis[1],axis[2]);
    }
    part_text_add(out,L"。\n");
    if(check->report_local)part_text_add(out,L"%ls抽检有 %d 个实体面出现局部差异。\n",side,check->differences);
}

static ProError imitation_check_body_faces(ProMdl source, ProMdl target, double source_mm,
                                           double target_mm, const ProOutline *bounds,int report_local,
                                           ImitationBoundaryCheck *check) {
    *check = (ImitationBoundaryCheck){.source=source,.target=target,.source_mm=source_mm,
        .target_mm=target_mm,.target_bounds=bounds,.report_local=report_local};
    ProSolidBody *bodies = NULL;
    int count = 0;
    ProError error = ProSolidBodiesCollect((ProSolid)source, &bodies);
    if (!error) error = ProArraySizeGet((ProArray)bodies, &count);
    for (int i = 0; !error && i < count; i++) {
        ProSolidBodyState state;
        error = ProSolidBodyStateGet(&bodies[i], &state);
        if (!error && state == PRO_BODY_STATE_ACTIVE)
            error = ProSolidBodySurfaceVisit(&bodies[i], imitation_face_samples, check);
    }
    if (bodies) ProArrayFree((ProArray *)&bodies);
    return error?error:check->differences?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}
