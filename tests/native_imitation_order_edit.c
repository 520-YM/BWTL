#define user_initialize frame_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

typedef struct {
    FabTemplate loop;
    struct { double x, y, diameter; } holes[64];
    int count, inner, outer;
} FrameHoles;

static ProError frame_hole_contour(ProContour contour, ProError status, ProAppData data) {
    if (status) return status;
    FrameHoles *frame = data;
    ProContourTraversal traversal;
    ProError error = ProContourTraversalGet(contour, &traversal);
    if (error) return error;
    if (traversal == PRO_CONTOUR_TRAV_EXTERNAL) {
        frame->outer++;
        return PRO_TK_NO_ERROR;
    }
    if (traversal != PRO_CONTOUR_TRAV_INTERNAL) return PRO_TK_BAD_CONTEXT;
    frame->inner++;
    frame->loop.internal = frame->loop.circular = 1;
    frame->loop.edges = 0;
    frame->loop.length = 0;
    error = ProContourEdgeVisit(frame->loop.projection->surface, contour, fab_template_edge, NULL, &frame->loop);
    if (error || !frame->loop.circular || !frame->loop.edges) return error;
    if (fabs(frame->loop.length - 2 * AS_PI * frame->loop.radius) > 0.0001) return PRO_TK_NO_ERROR;
    if (frame->count == _countof(frame->holes)) return PRO_TK_OUT_OF_RANGE;
    int i = frame->count++;
    frame->holes[i].x = frame->loop.center[0];
    frame->holes[i].y = frame->loop.center[1];
    frame->holes[i].diameter = 2 * frame->loop.radius;
    return PRO_TK_NO_ERROR;
}

static ProError frame_measure_holes(ProMdl model, FrameHoles *frame) {
    ImitationShapeMetrics shape;
    ProError error = imitation_shape_read(model, &shape);
    if (error) return error;
    ImitationWallFace face = {0};
    face.model = model;
    face.mm = shape.mm;
    face.axis = 2;
    face.low = shape.bounds[0][2];
    face.high = shape.bounds[1][2];
    ProSolidBody body;
    error = qa_single_body(model, &body);
    if (!error) error = ProSolidBodySurfaceVisit(&body, imitation_wall_pick, &face);
    if (error || !face.surface) return error ? error : PRO_TK_E_NOT_FOUND;
    ShDxf *projection = calloc(1, sizeof(*projection));
    if (!projection) return PRO_TK_OUT_OF_MEMORY;
    projection->surface = face.surface;
    projection->scale = 1 / shape.mm;
    projection->origin[2] = face.location / shape.mm;
    projection->u[0] = projection->v[1] = projection->normal[2] = 1;
    frame->loop.projection = projection;
    error = ProSurfaceContourVisit(face.surface, frame_hole_contour, NULL, frame);
    frame->loop.projection = NULL;
    free(projection);
    return error;
}

typedef struct {
    ProDimension dimension;
    ProDimensiontype type;
    double value;
    int found;
} OrderDimension;

static ProError order_dimension_visit(ProDimension *dim, ProError status, ProAppData data) {
    if (status) return status;
    OrderDimension *wanted = data;
    ProDimensiontype type;
    double value;
    ProError error = ProDimensionTypeGet(dim, &type);
    if (!error) error = ProDimensionValueGet(dim, &value);
    if (error) return error;
    if (type == wanted->type && fabs(value - wanted->value) < 0.0001) {
        wanted->dimension = *dim;
        wanted->found++;
    }
    return PRO_TK_NO_ERROR;
}

static ProError order_find_dimension(ProMdl model, const wchar_t *name, OrderDimension *wanted) {
    ProFeature feature;
    ProError error = ux_named_feature(model, name, &feature);
    if (!error) error = ProFeatureDimensionVisit(&feature, order_dimension_visit, NULL, wanted);
    return error ? error : wanted->found == 1 ? PRO_TK_NO_ERROR : PRO_TK_INCOMPLETE;
}

static ProError order_test_tree(ProMdl model) {
    ProFeature wall, seed, header, cut, pattern_header, *members = NULL;
    ProPattern pattern;
    ProFeattype wall_type, seed_type, cut_type;
    int wall_number, seed_number, header_number, cut_number, count;
    ProError error = ux_named_feature(model, L"FIRST_WALL", &wall);
    if (!error) error = ux_named_feature(model, L"HOLE_CUT", &seed);
    if (!error) error = ux_named_feature(model, L"HOLE_PATTERN", &header);
    if (!error) error = ux_named_feature(model, L"WINDOW_CUT", &cut);
    if (!error) error = ProFeatureTypeGet(&wall, &wall_type);
    if (!error) error = ProFeatureTypeGet(&seed, &seed_type);
    if (!error) error = ProFeatureTypeGet(&cut, &cut_type);
    if (!error) error = ProFeatureNumberGet(&wall, &wall_number);
    if (!error) error = ProFeatureNumberGet(&seed, &seed_number);
    if (!error) error = ProFeatureNumberGet(&header, &header_number);
    if (!error) error = ProFeatureNumberGet(&cut, &cut_number);
    if (!error) error = ProFeaturePatternGet(&seed, PRO_FEAT_PATTERN, &pattern);
    if (!error) error = ProPatternHeaderGet(&pattern, &pattern_header);
    if (!error) error = ProPatternMembersGet(&pattern, &members);
    if (!error) error = ProArraySizeGet((ProArray)members, &count);
    if (!error && (wall_type != PRO_FEAT_DATUM_SURF || seed_type != PRO_FEAT_CUT_SMT ||
        cut_type != PRO_FEAT_CUT_SMT || pattern_header.id != header.id || count != 14 ||
        wall_number >= seed_number || wall_number >= header_number ||
        seed_number >= cut_number || header_number >= cut_number))
        error = PRO_TK_INCOMPLETE;
    if (members) ProArrayFree((ProArray *)&members);
    return error;
}

static ProError order_test_hole_edit(ProMdl model, const FrameHoles *original, FILE *log) {
    OrderDimension diameter = {0};
    diameter.type = PRODIMTYPE_DIAMETER;
    diameter.value = original->holes[0].diameter;
    ProError error = order_find_dimension(model, L"HOLE_PROFILE", &diameter);
    fprintf(log, "hole_dimension_status=%d, matches=%d, old=%.12g\n", error, diameter.found, diameter.value);
    if (error) return error;
    const double new_diameter = diameter.value + 0.5;
    error = ProDimensionValueSet(&diameter.dimension, new_diameter);
    if (!error) error = part_regenerate(model);
    FrameHoles after = {0};
    if (!error) error = frame_measure_holes(model, &after);
    if (!error && after.count != original->count) error = PRO_TK_INCOMPLETE;
    for (int i = 0; !error && i < original->count; i++) {
        int found = 0;
        for (int j = 0; j < after.count; j++) {
            if (fabs(original->holes[i].x - after.holes[j].x) < 0.0001 &&
                fabs(original->holes[i].y - after.holes[j].y) < 0.0001 &&
                fabs(after.holes[j].diameter - new_diameter) < 0.0001) found++;
        }
        if (found != 1) error = PRO_TK_INCOMPLETE;
    }
    ProError restore = ProDimensionValueSet(&diameter.dimension, diameter.value);
    if (!restore) restore = part_regenerate(model);
    return error ? error : restore;
}

static ProError order_test_window_edit(ProMdl model, FILE *log) {
    OrderDimension width = {0};
    width.type = PRODIMTYPE_LINEAR;
    width.value = 280.0;
    ProError error = order_find_dimension(model, L"WINDOW_PROFILE", &width);
    fprintf(log, "window_dimension_status=%d, matches=%d, old=%.12g\n", error, width.found, width.value);
    if (error) return error;
    ProMassProperty before, after;
    error = qa_geometry_mass(model, &before);
    if (!error) error = ProDimensionValueSet(&width.dimension, 278);
    if (!error) error = part_regenerate(model);
    if (!error) error = qa_geometry_mass(model, &after);
    if (!error && fabs((after.volume - before.volume) - 2318) > 0.01) {
        fprintf(log, "window_volume_delta=%.12g\n", after.volume - before.volume);
        error = PRO_TK_INCOMPLETE;
    }
    ProError restore = ProDimensionValueSet(&width.dimension, width.value);
    if (!restore) restore = part_regenerate(model);
    return error ? error : restore;
}


static int frame_reopen(uiCmdCmdId id, uiCmdValue *value, void *data) {
    FILE *log = fopen("frame-order-edit.log", "w");
    if (!log) { ProEngineerEnd(); return 0; }
    ProMdl result = NULL, original = NULL;
    ProPath path;
    ProError error = PRO_TK_E_NOT_FOUND;
    if (GetEnvironmentVariableW(L"BWTL_FRAME_RESULT", path, _countof(path)))
        error = ProMdlFiletypeLoad(path, PRO_MDLFILE_PART, PRO_B_FALSE, &result);
    if (!error) error = dv_no_external_dependencies(result);
    double unit = 0, thickness = 0;
    if (!error) error = fab_thickness(result, &unit, &thickness);
    ProModelitem sketch;
    if (!error) error = ProModelitemByNameInit(result, PRO_FEATURE, L"OUTER_PROFILE", &sketch);
    FrameHoles before = {0}, after = {0};
    if (!error) error = frame_measure_holes(result, &after);
    if (!error && (fabs(thickness - 1) > 0.001 || after.count != 14 || after.inner != 15 || after.outer != 1))
        error = PRO_TK_INCOMPLETE;
    fprintf(log, "STANDALONE=%d, THICKNESS=%.12g, HOLES=%d, OTHER_INNER=%d, OUTER=%d\n",
            error, thickness, after.count, after.inner-after.count, after.outer);
    if (!error && GetEnvironmentVariableW(L"BWTL_FRAME_ORIGINAL", path, _countof(path)))
        error = ProMdlFiletypeLoad(path, PRO_MDLFILE_PART, PRO_B_FALSE, &original);
    if (!error && !original) error = PRO_TK_E_NOT_FOUND;
    if (!error) error = frame_measure_holes(original, &before);
    if (!error && before.count != after.count) error = PRO_TK_INCOMPLETE;
    int used[64] = {0};
    for (int i = 0; !error && i < before.count; i++) {
        int matched = 0;
        for (int j = 0; j < after.count; j++) {
            if (used[j] || fabs(before.holes[i].x - after.holes[j].x) > 0.0001 ||
                fabs(before.holes[i].y - after.holes[j].y) > 0.0001 ||
                fabs(before.holes[i].diameter - after.holes[j].diameter) > 0.0001) continue;
            used[j] = matched = 1;
            break;
        }
        if (!matched) error = PRO_TK_INCOMPLETE;
    }
    fprintf(log, "HOLE_POSITIONS_AND_DIAMETERS=%d\n", error);
    ProError tree = error ? error : order_test_tree(result);
    fprintf(log, "NATIVE_TREE_ORDER=%d\n", tree);
    ProError hole = tree ? tree : order_test_hole_edit(result, &after, log);
    fprintf(log, "HOLE_DIAMETER_PATTERN_EDIT=%d\n", hole);
    ProError window = tree ? tree : order_test_window_edit(result, log);
    fprintf(log, "WINDOW_DIMENSION_EDIT=%d\nDONE=1\n", window);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc, char **argv, char *version, char *build, wchar_t error_buffer[80]) {
    ProError error = frame_production_initialize(argc, argv, version, build, error_buffer);
    uiCmdCmdId id;
    if (!error) error = ProCmdActionAdd("BWTLFrameReopen", frame_reopen, uiProe2ndImmediate,
                                      NULL, PRO_B_TRUE, PRO_B_TRUE, &id);
    return error ? error : ProMacroLoad(L"~ Command `BWTLFrameReopen`;");
}
