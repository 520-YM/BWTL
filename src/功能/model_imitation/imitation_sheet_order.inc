typedef struct {
    const ImitationWallFace *face;
    FabTemplate loop;
    ProContour outside, window;
    int outer_count, window_count, holes, mixed_diameters;
    ProVector centers[64];
    double radius;
} ImitationFlatOrder;

static ProError imitation_order_contour(ProContour contour, ProError status, ProAppData data) {
    if (status) return status;
    ImitationFlatOrder *order = data;
    ProContourTraversal traversal;
    ProError error = ProContourTraversalGet(contour, &traversal);
    if (error) return error;
    if (traversal == PRO_CONTOUR_TRAV_EXTERNAL) {
        order->outside = contour;
        order->outer_count++;
        return PRO_TK_NO_ERROR;
    }
    if (traversal != PRO_CONTOUR_TRAV_INTERNAL) return PRO_TK_INVALID_TYPE;
    order->loop.internal = order->loop.circular = 1;
    order->loop.edges = 0;
    order->loop.length = 0;
    error = ProContourEdgeVisit(order->face->surface, contour, fab_template_edge, NULL, &order->loop);
    if (error) return error;
    if (!order->loop.circular || !order->loop.edges ||
        fabs(order->loop.length - 2 * AS_PI * order->loop.radius) > 0.0001) {
        order->window = contour;
        order->window_count++;
        return PRO_TK_NO_ERROR;
    }
    if (order->holes == _countof(order->centers)) return PRO_TK_OUT_OF_RANGE;
    if (!order->holes) order->radius = order->loop.radius;
    if (fabs(order->radius - order->loop.radius) > 1e-6) order->mixed_diameters = 1;
    double *point = order->centers[order->holes++];
    int axis = order->face->axis;
    point[axis] = order->face->location;
    point[(axis + 1) % 3] = order->loop.center[0];
    point[(axis + 2) % 3] = order->loop.center[1];
    return PRO_TK_NO_ERROR;
}

static ProError imitation_order_read(const ImitationWallFace *face, ImitationFlatOrder *order) {
    ShDxf *projection = calloc(1, sizeof(*projection));
    if (!projection) return PRO_TK_OUT_OF_MEMORY;
    order->face = face;
    order->loop.projection = projection;
    projection->surface = face->surface;
    projection->scale = 1 / face->mm;
    int axis = face->axis;
    projection->origin[axis] = face->location / face->mm;
    projection->u[(axis + 1) % 3] = projection->v[(axis + 2) % 3] = projection->normal[axis] = 1;
    ProError error = ProSurfaceContourVisit(face->surface, imitation_order_contour, NULL, order);
    order->loop.projection = NULL;
    free(projection);
    return error;
}

typedef struct {
    const ImitationFlatOrder *order;
    ProContour contour;
    int points;
} ImitationOrderedSketch;

static ProError imitation_order_sketch(ProSection section, const ProMatrix frame, double scale, void *data) {
    const ImitationOrderedSketch *request = data;
    const ImitationFlatOrder *order = request->order;
    if (request->contour) {
        ImitationFlatSection draw = {0};
        draw.face = order->face;
        draw.section = section;
        draw.frame = frame;
        draw.ratio = order->face->mm * scale;
        return ProContourEdgeVisit(order->face->surface, request->contour, imitation_draw_flat_edge, NULL, &draw);
    }
    ProBoolean intent;
    ProError error = ProSectionIntentManagerModeGet(section, &intent);
    if (!error && intent) error = ProSectionIntentManagerModeSet(section, PRO_B_FALSE);
    for (int i = 0; !error && i < (request->points ? order->holes : 1); i++) {
        ProVector delta;
        for (int j = 0; j < 3; j++) delta[j] = order->centers[i][j] * scale - frame[3][j];
        double x = ad_dot(delta, frame[0]), y = ad_dot(delta, frame[1]);
        int id;
        if (request->points) {
            Pro2dPointdef point = {0};
            point.type = PRO_2D_POINT;
            point.pnt[0] = x;
            point.pnt[1] = y;
            error = ProSectionEntityAdd(section, (Pro2dEntdef *)&point, &id);
        } else {
            error = model_circle(section, x, y, order->radius * scale, &id);
            if (!error) error = model_section_dimension(section, id, PRO_TK_DIM_DIA,
                                                        2 * order->radius * scale, x + 8 * scale, y + 8 * scale);
        }
    }
    return error;
}

static ProError imitation_order_pattern(ProMdl model, const wchar_t *name, int expected) {
    PartCommand command;
    wchar_t protocol[300];
    if (swprintf_s(protocol, _countof(protocol),
                   L"CREO_OP REUSE_POINTS %ls HOLE_PATTERN HOLE_CUT SKETCH:HOLE_LAYOUT", name) < 0 ||
        part_command_parse(protocol, &command) != 1) return PRO_TK_BAD_INPUTS;
    FdTxn tx = {0};
    ProError error = sh_begin(&tx, model, L"HOLE_PATTERN", PRO_FEAT_GROUP_HEAD);
    if (!error) error = ux_pattern(&tx, &command, UX_POINTS);
    fd_cleanup(&tx, error);
    if (error) return error;

    ProFeature seed, *members = NULL;
    ProPattern pattern;
    int count = 0;
    error = ux_named_feature(model, L"HOLE_CUT", &seed);
    if (!error) error = ProFeaturePatternGet(&seed, PRO_FEAT_PATTERN, &pattern);
    if (!error) error = ProPatternMembersGet(&pattern, &members);
    if (!error) error = ProArraySizeGet((ProArray)members, &count);
    if (members) ProArrayFree((ProArray *)&members);
    return error ? error : count == expected ? PRO_TK_NO_ERROR : PRO_TK_INCOMPLETE;
}

static ProError imitation_order_plane(ProMdl model, const wchar_t *base, double offset) {
    FdTxn tx = {0};
    FdRef reference = {0};
    ProElement constraints, entry;
    ProError error = sh_begin(&tx, model, L"WALL_PLANE", PRO_FEAT_DATUM);
    if (!error) error = fd_ref_resolve(model, base, PRO_SURFACE, &reference);
    if (!error) error = model_element(tx.root, PRO_E_DTMPLN_CONSTRAINTS, 0, 0, &constraints);
    if (!error) error = model_element(constraints, PRO_E_DTMPLN_CONSTRAINT, 0, 0, &entry);
    if (!error) error = model_element(entry, PRO_E_DTMPLN_CONSTR_TYPE, 1, PRO_DTMPLN_OFFS, NULL);
    if (!error) error = fd_ref_element(&tx, entry, PRO_E_DTMPLN_CONSTR_REF, &reference);
    if (!error) error = model_element(entry, PRO_E_DTMPLN_CONSTR_REF_OFFSET, 2, offset * tx.scale, NULL);
    if (!error) error = fd_create(&tx, 0);
    if (!error) error = fd_verify(&tx);
    fd_cleanup(&tx, error);
    fd_ref_free(&reference);
    return error;
}


static ProError imitation_ordered_sheet(const ImitationFlatOrder *order, double thickness,
                                        ProMdl *result, PartText *out) {
    const ImitationWallFace *face = order->face;
    ProMdlName name;
    const char *stage = "ordered sheet template";
    ProError error = model_create_empty(2, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, result);
    if (error) return error;
    const wchar_t *plane = imitation_template_plane(*result, face->axis);
    const wchar_t *orient = imitation_template_plane(*result, (face->axis + 1) % 3);
    if (!plane || !orient) return PRO_TK_E_NOT_FOUND;
    ProVector point, normal;
    double mm;
    error = qa_mm_per_unit(*result, &mm);
    if (!error) error = imitation_plane(*result, plane, point, normal);
    if (error) return error;
    double offset = (face->location - point[face->axis] * mm) / normal[face->axis];
    if (fabs(offset) > 1e-6) {
        /* Create a datum through the same native sheet-capable transaction used by sketches. */
        error = imitation_order_plane(*result, plane, offset);
        if (error) return error;
        plane = L"PLANE:WALL_PLANE";
    }

    ProMatrix frame;
    ImitationOrderedSketch outline = {order, order->outside, 0};
    ImitationOrderedSketch locations = {order, NULL, 1};
    ImitationOrderedSketch seed = {order, NULL, 0};
    ImitationOrderedSketch window = {order, order->window, 0};
    int direction = fabs(face->location - face->low) <= 0.001 ? 1 : -1;
#define ORDER_STEP(call) do { error = (call); trace(#call, error); if (error) return error; } while (0)
    ORDER_STEP(smx_custom_sketch(*result, L"OUTER_PROFILE", plane, orient, imitation_order_sketch, &outline, frame));
    int forward = frame[2][face->axis] * direction > 0;
    const wchar_t *wall_sense = forward ? L"POS" : L"NEG";
    const wchar_t *cut_sense = forward ? L"NEG" : L"POS";
    ORDER_STEP(imitation_operation(*result, out, 0,
        L"CREO_OP SMT_FIRST_WALL %ls FIRST_WALL SKETCH:OUTER_PROFILE %.12g %ls", name, thickness, wall_sense));
    ORDER_STEP(smx_custom_sketch(*result, L"HOLE_LAYOUT", plane, orient, imitation_order_sketch, &locations, frame));
    ORDER_STEP(smx_custom_sketch(*result, L"HOLE_PROFILE", plane, orient, imitation_order_sketch, &seed, frame));
    ORDER_STEP(imitation_operation(*result, out, 0, L"CREO_OP SMT_CUT %ls HOLE_CUT SKETCH:HOLE_PROFILE %ls", name, cut_sense));
    ORDER_STEP(imitation_order_pattern(*result, name, order->holes));
    ORDER_STEP(smx_custom_sketch(*result, L"WINDOW_PROFILE", plane, orient, imitation_order_sketch, &window, frame));
    ORDER_STEP(imitation_operation(*result, out, 0, L"CREO_OP SMT_CUT %ls WINDOW_CUT SKETCH:WINDOW_PROFILE %ls", name, cut_sense));
#undef ORDER_STEP
    part_text_add(out, L"按首壁 → 圆孔拉伸切口及点阵列 → 窗口拉伸切口重建。板厚 %.8g mm，阵列 %d 孔，孔径和窗口各用独立草绘。\n",
                  thickness, order->holes);
    return PRO_TK_NO_ERROR;
}
