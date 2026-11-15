#define user_initialize imitation_test_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int checks, failures;

static void result(const wchar_t *label, ProError error, const wchar_t *report) {
    checks++;
    failures += error != PRO_TK_NO_ERROR;
    FILE *file = _wfopen(L"universal-imitation.log", L"at, ccs=UTF-8");
    if (file) {
        fwprintf(file, L"%ls=%d\n%ls\n", label, error, report);
        fclose(file);
    }
}

static ProError imitate_and_reopen(ProMdl source, const wchar_t *expected, PartText *out) {
    ProPath path;
    ProError error = imitation_run(source, path, 0, out);
    if (error) return error;
    if (!wcsstr(out->text, expected) || !dv_file_nonempty(path)) return PRO_TK_INCOMPLETE;
    ProMdl reopened = NULL;
    error = ProMdlFiletypeLoad(path, PRO_MDLFILE_PART, PRO_B_FALSE, &reopened);
    if (!error) error = dv_no_external_dependencies(reopened);
    ImitationShapeMetrics before;
    if (!error) error = imitation_shape_read(source, &before);
    if (!error) error = imitation_shape_verify(source, reopened, &before, out);
    if (reopened) {
        ProError erase = ProMdlErase(reopened);
        if (!error) error = erase;
    }
    return error;
}

static int run_imitation_test(uiCmdCmdId id, uiCmdValue *value, void *data) {
    wchar_t buffer[9000] = L"";
    PartText out = {buffer, _countof(buffer), 0};
    ProPath source_path, final;
    ProMdl sheet = NULL, native = NULL, imported = NULL, blank = NULL;
    ProMdlName name;
    const char *stage = "prepare test";
    model_background_mode = 1;

    ProError error = PRO_TK_E_NOT_FOUND;
    if (GetEnvironmentVariableW(L"BWTL_TEST_UNRELATED_SHEET", source_path, _countof(source_path)))
        error = ProMdlFiletypeLoad(source_path, PRO_MDLFILE_PART, PRO_B_FALSE, &sheet);
    if (!error) error = imitate_and_reopen(sheet, L"原生特征保留", &out);
    result(L"NATIVE_SHEET_REOPEN", error, buffer);

    buffer[0] = 0;
    ModelPlan cylinder = {1, 41, 41, 23, 0, 1};
    error = model_create(&cylinder, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, &native);
    if (!error) error = imitate_and_reopen(native, L"原生特征保留", &out);
    result(L"NATIVE_CYLINDER_REOPEN", error, buffer);

    buffer[0] = 0;
    DeliveryOutput work = {0};
    error = native ? fab_scratch(&work) : PRO_TK_BAD_CONTEXT;
    if (!error) error = dv_stage_enter(&work);
    if (!error) error = imitation_step_new(native, &work, L"external_cylinder", &imported);
    error = fab_discard(&work, error, &out);
    if (!error) error = imitate_and_reopen(imported, L"实际边界几何", &out);
    result(L"IMPORTED_CYLINDER_REOPEN", error, buffer);

    buffer[0] = 0;
    ProMdl surfaces = NULL;
    error = model_create_empty(1, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, &surfaces);
    if (!error) error = imitation_operation(surfaces, &out, 0,
        L"CREO_OP SKETCH_CREATE %ls WALL_SK PLANE:FRONT PLANE:RIGHT ARC 10 10 10 0 180", name);
    if (!error) error = imitation_operation(surfaces, &out, 0,
        L"CREO_OP SURFACE_EXTRUDE %ls WALL WALL_SK 15 POS", name);
    if (!error) error = imitate_and_reopen(surfaces, L"1 个面组", &out);
    result(L"CURVED_QUILT_REOPEN", error, buffer);

    buffer[0] = 0;
    ProMdl multiple = NULL;
    error = model_create_empty(1, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, &multiple);
    if (!error) error = imitation_operation(multiple, &out, 0,
        L"CREO_OP SKETCH_CREATE %ls A_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 10 12", name);
    if (!error) error = imitation_operation(multiple, &out, 0,
        L"CREO_OP BODY_EXTRUDE %ls BODY_A A_SK 10 POS", name);
    if (!error) error = imitation_operation(multiple, &out, 0,
        L"CREO_OP SKETCH_CREATE %ls B_SK PLANE:FRONT PLANE:RIGHT RECT 25 0 10 12", name);
    if (!error) error = imitation_operation(multiple, &out, 0,
        L"CREO_OP BODY_EXTRUDE %ls BODY_B B_SK 10 POS", name);
    if (!error) error = imitate_and_reopen(multiple, L"2 个实体", &out);
    result(L"MULTIBODY_REOPEN", error, buffer);


    buffer[0] = 0;
    error = model_create_empty(1, name, &stage);
    if (!error) error = ProMdlnameInit(name, PRO_MDL_PART, &blank);
    if (!error) {
        ProError rejected = imitation_run(blank, final, 0, &out);
        if (!rejected || final[0]) error = PRO_TK_INCOMPLETE;
    }
    result(L"BLANK_NO_PUBLICATION", error, buffer);

    buffer[0] = 0;
    ProMdl frame = NULL, result_model = NULL;
    ProPath frame_path;
    error = PRO_TK_E_NOT_FOUND;
    if (GetEnvironmentVariableW(L"BWTL_TEST_MISSING_REFERENCE_FRAME", frame_path, _countof(frame_path)))
        error = ProMdlFiletypeLoad(frame_path, PRO_MDLFILE_PART, PRO_B_FALSE, &frame);
    if (!error && !dv_no_external_dependencies(frame)) error = PRO_TK_INCOMPLETE;
    if (!error) error = imitation_run(frame, final, 0, &out);
    if (!error && !dv_file_nonempty(final)) error = PRO_TK_INCOMPLETE;
    if (!error) error = ProMdlFiletypeLoad(final, PRO_MDLFILE_PART, PRO_B_FALSE, &result_model);
    if (!error) error = dv_no_external_dependencies(result_model);
    double unit, gauge;
    if (!error) error = fab_thickness(result_model, &unit, &gauge);
    if (!error && fabs(gauge - 1.0) > 0.001) error = PRO_TK_INCOMPLETE;
    ImitationShapeMetrics original;
    if (!error) error = imitation_shape_read(frame, &original);
    if (!error) error = imitation_shape_verify(frame, result_model, &original, &out);
    if (!error) part_text_add(&out, L"FRAME_FILE=%ls\nFRAME_GAUGE_MM=%.12g\n", final, gauge);
    result(L"MISSING_REFERENCE_FRAME_REOPEN", error, buffer);


    buffer[0] = 0;
    DeliveryOutput fallback = {0};
    ProMdl step_frame = NULL;
    error = frame ? imitation_shape_read(frame, &original) : PRO_TK_BAD_CONTEXT;
    if (!error) error = fab_scratch(&fallback);
    if (!error) error = dv_stage_enter(&fallback);
    if (!error) error = imitation_step_new(frame, &fallback, L"bwtl_test_frame_step", &step_frame);
    ImitationShapeMetrics step_shape;
    if (!error) error = imitation_shape_read(step_frame, &step_shape);
    if (!error && (step_shape.body_shells != 1 || step_shape.quilts != original.quilts))
        error = PRO_TK_INCOMPLETE;
    if (!error) error = imitation_shape_verify(frame, step_frame, &original, &out);
    if (step_frame) {
        ProError erase = ProMdlErase(step_frame);
        if (!error) error = erase;
    }
    error = fab_discard(&fallback, error, &out);
    result(L"FRAME_STEP_DUPLICATE_SHELL_ACCEPTED", error, buffer);


    FILE *file = _wfopen(L"universal-imitation.log", L"at, ccs=UTF-8");
    if (file) {
        fwprintf(file, L"CHECKS=%d\nFAILURES=%d\nDONE=1\n", checks, failures);
        fclose(file);
    }
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc, char **argv, char *version, char *build, wchar_t error_buffer[80]) {
    char marker[16];
    if (!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST", marker, sizeof(marker)) || strcmp(marker, "yes"))
        return PRO_TK_BAD_CONTEXT;
    uiCmdCmdId id;
    ProError error = imitation_test_production_initialize(argc, argv, version, build, error_buffer);
    if (!error) error = ProCmdActionAdd("BWTLUniversalImitationTest", run_imitation_test,
                                      uiProe2ndImmediate, NULL, PRO_B_TRUE, PRO_B_TRUE, &id);
    return error ? error : ProMacroLoad(L"~ Command `BWTLUniversalImitationTest`;");
}
