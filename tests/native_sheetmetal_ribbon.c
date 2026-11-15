#define user_initialize ribbon_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProNotify.h>
#include <ProCore.h>

static const char *ribbon_modes[] = { "home", "part", "assembly", "sheet metal" };
static const int ribbon_model_kinds[] = { 0, 1, 3, 2 };
static const char ribbon_page[] = "BWTL_Assistant_Page";
static size_t ribbon_mode;
static unsigned ribbon_failures, ribbon_checks;
static int ribbon_phase, ribbon_selected;
static ULONGLONG ribbon_wait_started;

static void ribbon_check(int passed, const char *label) {
    ribbon_checks++;
    ribbon_failures += !passed;
    FILE *file = fopen("sheetmetal-ribbon.log", "a");
    if (file) {
        fprintf(file, "%s: %s\n", label, passed ? "PASS" : "FAIL");
        fclose(file);
    }
}

static ProError ribbon_switched(char *from_tab, char *to_tab) {
    if (to_tab && !strncmp(to_tab, ribbon_page, sizeof(ribbon_page) - 1)) {
        ribbon_selected = 1;
    }
    return PRO_TK_NO_ERROR;
}

static int ribbon_finish(void) {
    KillTimer(native_backend_window, 5739);
    FILE *file = fopen("sheetmetal-ribbon.log", "a");
    if (file) {
        fprintf(file, "RIBBON_CHECKS=%u\nRIBBON_FAILURES=%u\nRIBBON_DONE=1\n",
                ribbon_checks, ribbon_failures);
        fclose(file);
    }
    ProEngineerEnd();
    return 0;
}

static int ribbon_tick(uiCmdCmdId command, uiCmdValue *value, void *data) {
    if (ribbon_phase == 0) {
        if (ribbon_mode == _countof(ribbon_modes)) {
            ribbon_phase = 3;
            ribbon_wait_started = GetTickCount64();
            ProMacroLoad(L"~ Command `CodexNativeOpen`;");
            return 0;
        }
        int kind = ribbon_model_kinds[ribbon_mode];
        if (kind) {
            ProMdlName name;
            const char *stage = "open template";
            ProError error = model_create_empty(kind, name, &stage);
            if (error) {
                trace(stage, error);
                ribbon_check(0, "open test model");
                return ribbon_finish();
            }
        }
        ribbon_phase = 1;
        return 0;
    }
    if (ribbon_phase == 1) {
        if (ribbon_model_kinds[ribbon_mode] == 2) {
            ProMdl model = NULL;
            ProMdlsubtype subtype;
            ribbon_check(!ProMdlCurrentGet(&model) && model &&
                         !ProMdlSubtypeGet(model, &subtype) &&
                         subtype == PROMDLSTYPE_PART_SHEETMETAL, "active native sheet metal part");
        }
        ribbon_selected = 0;
        ribbon_wait_started = GetTickCount64();
        ribbon_phase = 2;
        ProMacroLoad(L"~ Activate `main_dlg_cur` `BWTL_Assistant_Page_control_btn` 1;");
        return 0;
    }
    if (ribbon_phase == 2) {
        if (!ribbon_selected && GetTickCount64() - ribbon_wait_started < 3500) return 0;
        ribbon_check(ribbon_selected, ribbon_modes[ribbon_mode++]);
        ribbon_phase = 0;
        return 0;
    }
    if (!native_window && GetTickCount64() - ribbon_wait_started < 3500) return 0;
    ribbon_check(native_window && IsWindowVisible(native_window), "native assistant visible in sheet metal");
    return ribbon_finish();
}

static void CALLBACK ribbon_timer(HWND window, UINT message, UINT_PTR id, DWORD now) {
    ProMacroLoad(L"~ Command `BWTLRibbonProbeTick`;");
}

__declspec(dllexport) int user_initialize(int argc, char **argv, char *version, char *build,
                                          wchar_t error_buffer[80]) {
    char enabled[16];
    if (!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST", enabled, sizeof(enabled)) ||
        strcmp(enabled, "yes")) return PRO_TK_BAD_CONTEXT;
    uiCmdCmdId id;
    ProError error = ribbon_production_initialize(argc, argv, version, build, error_buffer);
    if (!error) error = ProNotificationSet(PRO_RIBBON_TAB_SWITCH, (ProFunction)ribbon_switched);
    if (!error) error = ProCmdActionAdd("BWTLRibbonProbeTick", ribbon_tick, uiProe2ndImmediate,
                                       NULL, PRO_B_TRUE, PRO_B_TRUE, &id);
    if (error) return error;
    if (!SetTimer(native_backend_window, 5739, 600, ribbon_timer)) return PRO_TK_GENERAL_ERROR;
    return PRO_TK_NO_ERROR;
}
