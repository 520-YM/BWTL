typedef struct {
    ProMdl model;
    ProSurface surface;
    double mm, area, low, high, location;
    int axis, id;
} ImitationWallFace;

static ProError imitation_wall_pick(ProSurface surface, ProError status, ProAppData data) {
    if (status) return status;
    ImitationWallFace *pick = data;
    ProSrftype type;
    ProError error = ProSurfaceTypeGet(surface, &type);
    if (error || type != PRO_SRF_PLANE) return error;
    double area;
    error = ProSurfaceAreaEval(surface, &area);
    if (error || area <= pick->area) return error;
    double location = 0;
    if (pick->axis >= 0) {
        ProGeomitem item;
        ProVector point, normal;
        error = ProSurfaceToGeomitem((ProSolid)pick->model, surface, &item);
        if (!error) error = fd_plane_data(&item, point, normal);
        if (error) return error;
        if (fabs(normal[pick->axis]) < 0.999999) return PRO_TK_NO_ERROR;
        location = point[pick->axis] * pick->mm;
        if (fabs(location - pick->low) > 0.001 && fabs(location - pick->high) > 0.001)
            return PRO_TK_NO_ERROR;
    }
    error = ProSurfaceIdGet(surface, &pick->id);
    if (!error) {
        pick->surface = surface;
        pick->area = area;
        pick->location = location;
    }
    return error;
}

static ProError imitation_sheet_convert(ProMdl model, double thickness, PartText *out) {
    ProSolidBody body;
    ImitationWallFace face = {0};
    face.model = model;
    face.axis = -1;
    ProMdlName name;
    ProError error = qa_single_body(model, &body);
    if (!error) error = ProSolidBodySurfaceVisit(&body, imitation_wall_pick, &face);
    if (!error && !face.id) error = PRO_TK_E_NOT_FOUND;
    if (!error) error = ProMdlMdlnameGet(model, name);
    if (!error) error = imitation_operation(model, out, 0,
        L"CREO_OP SMT_CONVERT %ls BWTL_SHEET SURFACE:%d %.12g", name, face.id, thickness);
    double mm, actual;
    if (!error) error = fab_thickness(model, &mm, &actual);
    if (!error && fabs(actual - thickness) > 0.001) error = PRO_TK_INCOMPLETE;
    return error;
}

typedef struct {
    const ImitationWallFace *face;
    ProSection section;
    const double (*frame)[4];
    double ratio;
    int edges, contours;
} ImitationFlatSection;

static ProError imitation_project_edge(const ImitationFlatSection *draw, const double point[3], AsPoint *xy) {
    ProVector delta;
    for (int i = 0; i < 3; i++) delta[i] = point[i] * draw->ratio - draw->frame[3][i];
    if (fabs(ad_dot(delta, draw->frame[2])) > 0.001 * draw->ratio / draw->face->mm)
        return PRO_TK_BAD_CONTEXT;
    *xy = as_point(ad_dot(delta, draw->frame[0]), ad_dot(delta, draw->frame[1]));
    return PRO_TK_NO_ERROR;
}

static ProError imitation_draw_flat_edge(ProEdge edge, ProError status, ProAppData data) {
    if (status) return status;
    ImitationFlatSection *draw = data;
    ProEnttype type;
    ProVector a, b;
    AsPoint first, last;
    int id;
    ProError error = ProEdgeTypeGet(edge, &type);
    if (!error) error = ProEdgeXyzdataEval(edge, 0, a, NULL, NULL, NULL);
    if (!error) error = ProEdgeXyzdataEval(edge, 1, b, NULL, NULL, NULL);
    if (!error) error = imitation_project_edge(draw, a, &first);
    if (!error) error = imitation_project_edge(draw, b, &last);
    if (error) return error;
    draw->edges++;
    if (type == PRO_ENT_LINE) return as_add_line(draw->section, first, last, &id);
    if (type != PRO_ENT_ARC && type != PRO_ENT_CIRCLE) return PRO_TK_INVALID_TYPE;

    ProGeomitemdata *raw = NULL;
    error = ProEdgeDataGet(edge, &raw);
    if (error) return error;
    const ProCurvedata *curve = raw->data.p_curve_data;
    AsPoint center;
    double radius = (type == PRO_ENT_ARC ? curve->arc.radius : curve->circle.radius) * draw->ratio;
    error = imitation_project_edge(draw, type == PRO_ENT_ARC ? curve->arc.origin : curve->circle.center, &center);
    ProGeomitemdataFree(&raw);
    if (error) return error;
    if (type == PRO_ENT_CIRCLE || as_dist(first, last) < 1e-8) {
        return model_circle(draw->section, center.x, center.y, radius, &id);
    }
    ProVector m;
    AsPoint middle;
    error = ProEdgeXyzdataEval(edge, 0.5, m, NULL, NULL, NULL);
    if (!error) error = imitation_project_edge(draw, m, &middle);
    if (error) return error;
    double start = atan2(first.y - center.y, first.x - center.x);
    double end = atan2(last.y - center.y, last.x - center.x);
    double mid = atan2(middle.y - center.y, middle.x - center.x);
    if (fx_positive_angle(mid - start) > fx_positive_angle(end - start)) {
        double swap = start;
        start = end;
        end = swap;
    }
    return as_add_arc(draw->section, center, radius, start, start + fx_positive_angle(end - start), &id);
}

static ProError imitation_draw_flat_contour(ProContour contour, ProError status, ProAppData data) {
    if (status) return status;
    ImitationFlatSection *draw = data;
    draw->contours++;
    return ProContourEdgeVisit(draw->face->surface, contour, imitation_draw_flat_edge, NULL, draw);
}

static ProError imitation_draw_flat(ProSection section, const ProMatrix frame, double scale, void *data) {
    ImitationFlatSection *draw = data;
    draw->section = section;
    draw->frame = frame;
    draw->ratio = draw->face->mm * scale;
    return ProSurfaceContourVisit(draw->face->surface, imitation_draw_flat_contour, NULL, draw);
}

#include "imitation_sheet_order.inc"

static ProError imitation_flat_new(ProMdl source, const ImitationShapeMetrics *shape, double thickness,
                                    ProMdl *result, PartText *out) {
    if (shape->bodies != 1 || shape->quilts) return PRO_TK_BAD_CONTEXT;
    ImitationWallFace face = {0};
    face.model = source;
    face.mm = shape->mm;
    face.axis = -1;
    for (int i = 0; i < 3; i++) {
        double extent = shape->bounds[1][i] - shape->bounds[0][i];
        if (fabs(extent - thickness) <= 0.001) {
            face.axis = i;
            face.low = shape->bounds[0][i];
            face.high = shape->bounds[1][i];
            break;
        }
    }
    if (face.axis < 0) return PRO_TK_BAD_CONTEXT;
    ProSolidBody body;
    ProError error = qa_single_body(source, &body);
    if (!error) error = ProSolidBodySurfaceVisit(&body, imitation_wall_pick, &face);
    if (error || !face.id) return error ? error : PRO_TK_E_NOT_FOUND;

    ImitationFlatOrder order = {0};
    error = imitation_order_read(&face, &order);
    if (!error && order.outer_count == 1 && order.window_count == 1 &&
        order.holes >= 2 && !order.mixed_diameters)
        return imitation_ordered_sheet(&order, thickness, result, out);


    ProMdlName name;
    const char *stage = "imitation flat template";
    error = model_create_empty(1, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, result);
    if (error) return error;
    const wchar_t *base = imitation_template_plane(*result, face.axis);
    const wchar_t *orient = imitation_template_plane(*result, (face.axis + 1) % 3);
    if (!base || !orient) return PRO_TK_E_NOT_FOUND;

    ProVector point, normal;
    double mm;
    error = qa_mm_per_unit(*result, &mm);
    if (!error) error = imitation_plane(*result, base, point, normal);
    if (error) return error;
    if (fabs(point[face.axis] * mm - face.location) > 1e-6) {
        double offset = (face.location - point[face.axis] * mm) / normal[face.axis];
        error = imitation_operation(*result, out, 0, L"CREO_OP DATUM_PLANE %ls BWTL_WALL_PLANE %ls %.12g",
                                    name, base, offset);
        if (error) return error;
        base = L"PLANE:BWTL_WALL_PLANE";
    }

    ImitationFlatSection draw = {0};
    draw.face = &face;
    ProMatrix frame;
    error = smx_custom_sketch(*result, L"BWTL_WALL_PROFILE", base, orient, imitation_draw_flat, &draw, frame);
    if (error || !draw.contours) return error ? error : PRO_TK_INCOMPLETE;
    int direction = fabs(face.location - face.low) <= 0.001 ? 1 : -1;
    const wchar_t *sense = frame[2][face.axis] * direction > 0 ? L"NEG" : L"POS";
    error = imitation_operation(*result, out, 1,
        L"CREO_OP SOLID_EXTRUDE %ls BWTL_FLAT BWTL_WALL_PROFILE ADD BLIND %.12g %ls", name, thickness, sense);
    if (!error) error = imitation_sheet_convert(*result, thickness, out);
    if (!error) part_text_add(out, L"从源壁面提取 %d 个轮廓、%d 条边，重建为可编辑草绘和 %.8g mm 原生钣金。\n",
                              draw.contours, draw.edges, thickness);
    return error;
}
