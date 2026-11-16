#define BWTL_SKETCH_RECOVERY_TEST
#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProMdl expected;
static ProMdlName expected_name;
static ULONGLONG started;
static int queued,forced,recovered;
static FILE *report;

static int fixture_finish(int pass){
    KillTimer(native_backend_window,6412);
    if(report){
        fprintf(report,"FORCED=%d RECOVERY_ACCEPTED=%d WRITES=%d FAILURES=%d PASS=%d\n",
            forced,recovered,smart_progress,smart_total_failures,pass);
        fclose(report);
        report=NULL;
    }
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int fixture_verify(uiCmdCmdId id,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()-started>190000)return fixture_finish(0);
    if(smart_active||ai_process)return 0;
    ProMdl active=NULL;
    ProFeature legs;
    ProSolidBody body;
    ProOutline outline;
    ProMassProperty mass;
    const wchar_t *answer=wcsstr(history,L"【AI】");
    int pass=!ProMdlCurrentGet(&active)&&(!forced||active==expected)&&
        !ux_named_feature(active,L"QP_STOOL_LEGS",&legs)&&!qa_single_body(active,&body)&&
        !ProSolidBodyOutlineGet(&body,outline)&&!qa_geometry_mass(active,&mass)&&outline[0][2]<0&&
        smart_progress>=2&&answer&&!wcsstr(answer+4,L"【AI】")&&
        (!forced||(recovered&&smart_total_failures==1));
    if(report&&pass)fprintf(report,"ONE_REPLY=1 SAME_MODEL=1 LOW_Z=%g UPPER_Z=%g VOLUME=%g\n",
        outline[0][2],outline[1][2],mass.volume);
    return fixture_finish(pass);
}

static void CALLBACK fixture_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!queued)queued=!ProMacroLoad(L"~ Command \x0060" L"BWTLSketchRecoveryVerify\x0060;");
}

static int fixture_send(uiCmdCmdId id,uiCmdValue *value,void *data){
    if(!native_ui_open_threaded(NULL)||
       !SetTimer(native_backend_window,6412,350,fixture_timer))return fixture_finish(0);
    started=GetTickCount64();
    const wchar_t *goal=L"给我画张床，尺寸自己给";
    if(!forced){
        native_prompt_set(goal);
        run_local(NULL,NULL,NULL);
        return 0;
    }

    smart_begin(goal);
    smart_intent=SMART_INTENT_NEW;
    smart_readonly=0;
    smart_target_model=expected;
    ProPath directory;
    if(ProDirectoryCurrentGet(directory)||!smart_binding_set(expected,expected,directory)||
       part_snapshot_take_mode(expected,&request_snapshot,0))return fixture_finish(0);
    smart_progress=smart_steps=1;
    smart_record_tool(L"第 1 步｜成功｜新建 2000 × 1500 × 80 mm 床板。保留这个零件，下一步做四条腿。");
    wchar_t protocol[340];
    swprintf_s(protocol,_countof(protocol),L"CREO_OP SKETCH_CREATE %ls LegSketch1 PLANE:FRONT PLANE:RIGHT RECT 100 100 80 80",expected_name);
    if(part_command_parse(protocol,&pending_part)!=1)return fixture_finish(0);
    PartCommand rename=pending_part;
    wcscpy_s(rename.arg[2],PART_TOKEN_MAX,L"LegSketchNew");
    fd_test_first_sketch_failure=!GetEnvironmentVariableW(L"BWTL_SKETCH_RECOVERY_ORIGINAL",NULL,0);
    execute_part_command();
    recovered=smart_active&&smart_steps==2&&smart_total_failures==1&&
        smart_protocol_seen(&rename)&&wcsstr(smart_last_failure,L"已复核");
    fprintf(report,"FAILED_RENAME_BLOCKED=%d\n",smart_protocol_seen(&rename));
    return recovered?0:fixture_finish(0);
}

static int fixture_start(uiCmdCmdId id,uiCmdValue *value,void *data){
    report=fopen("sketch-recovery.log","w");
    if(!report)return fixture_finish(0);
    setvbuf(report,NULL,_IONBF,0);
    forced=!GetEnvironmentVariableW(L"BWTL_SKETCH_RECOVERY_LIVE",NULL,0);
    ProError error=0;
    if(forced){
        const ModelPlan plate={0,2000,1500,80,0,1};
        const char *stage=NULL;
        error=model_create(&plate,expected_name,&stage);
        if(!error)error=ProMdlnameInit(expected_name,PRO_MDL_PART,&expected);
        if(!error)error=dv_display_new(expected);
        fprintf(report,"BED_PLATE=%d\n",error);
    }
    if(!error)error=ProMacroLoad(L"~ Command \x0060" L"BWTLSketchRecoverySend\x0060;");
    return error?fixture_finish(0):0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    wchar_t codex[MAX_PATH];
    if(!error&&(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)))
        error=PRO_TK_E_NOT_FOUND;
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLSketchRecoveryStart",fixture_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLSketchRecoverySend",fixture_send,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLSketchRecoveryVerify",fixture_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLSketchRecoveryStart\x0060;");
}