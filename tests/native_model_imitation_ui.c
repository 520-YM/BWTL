#define user_initialize imitation_production_initialize
#include "../src/assistant.c"
#undef user_initialize

static int ui_cases,ui_failures,ui_phase;
static ULONGLONG ui_start;
static ProMdl ui_original;

static void imitation_ui_check(int pass,const char *label){
    ui_cases++;
    ui_failures+=!pass;
    FILE *file=fopen("imitation-ui.log","a");
    if(file){fprintf(file,"%s: %d\n",label,pass?0:-1);fclose(file);}
}

static int imitation_ui_finish(void){
    KillTimer(native_backend_window,5736);
    FILE *file=fopen("imitation-ui.log","a");
    if(file){
        fprintf(file,"UI_CASES: %d\nUI_FAILURES: %d\nUI_DONE: 1\n",ui_cases,ui_failures);
        fclose(file);
    }
    ProEngineerEnd();
    return 0;
}

static int imitation_ui_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(GetTickCount64()-ui_start>25000){
        imitation_ui_check(0,"UI request completed within timeout");
        return imitation_ui_finish();
    }
    if(native_busy||imitation_pending)return 0;
    if(ui_phase==0){
        if(!imitation_last.path[0])return 0;
        ProMdl model=NULL;
        ProMdlsubtype subtype;
        imitation_ui_check(dv_file_nonempty(imitation_last.path),"workdir result written");
        imitation_ui_check(!ProMdlCurrentGet(&model)&&model&&model!=ui_original&&
            !ProMdlSubtypeGet(model,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL,"separate native sheet metal becomes current");
        imitation_ui_check(!ai_process&&wcsstr(history,L"768 条有效实体射线"),"one native receipt without AI service");
        ui_phase=1;
        native_imitation_action(IMITATION_LIBRARY);
        return 0;
    }
    const wchar_t *first=wcsstr(history,L"【仿照模型】");
    const wchar_t *second=first?wcsstr(first+1,L"【仿照模型】"):NULL;
    if(!second)return 0;
    ProPath library;
    wchar_t *last=wcsrchr(imitation_last.path,L'\\');
    if(!last)return imitation_ui_finish();
    swprintf_s(library,_countof(library),L"%ls\\公司资料\\公司标准\\公司标准件%ls",runtime_root,last);
    imitation_ui_check(dv_file_nonempty(library),"company standard file saved through same menu action");
    if(dv_file_nonempty(library)){
        wchar_t report[800]=L"";
        PartText out={report,_countof(report),0};
        imitation_ui_check(imitation_save_library(&imitation_last,&out)==PRO_TK_E_FOUND,"second library save preserves existing model");
    }
    return imitation_ui_finish();
}

static void CALLBACK imitation_ui_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    ProMacroLoad(L"~ Command `BWTLImitationUiTick`;");
}

static int imitation_ui_start(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProPath source;
    wchar_t label[30]=L"";
    if(!GetEnvironmentVariableW(L"BWTL_IMITATION_TEST_INPUT",source,_countof(source))||
       ProMdlFiletypeLoad(source,PRO_MDLFILE_PART,PRO_B_FALSE,&ui_original)||
       dv_display_new(ui_original)||!native_ui_open_threaded(NULL)){
        imitation_ui_check(0,"open source and production chat in private session");
        return imitation_ui_finish();
    }
    GetWindowTextW(native_top_buttons[3],label,_countof(label));
    imitation_ui_check(!wcscmp(label,L"仿照模型")&&IsWindowEnabled(native_top_buttons[3]),"fourth toolbar button is enabled with the requested label");
    ui_start=GetTickCount64();
    if(!SetTimer(native_backend_window,5736,500,imitation_ui_timer)){
        imitation_ui_check(0,"create private verification timer");
        return imitation_ui_finish();
    }
    native_imitation_action(IMITATION_CURRENT);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    uiCmdCmdId id;
    ProError error=imitation_production_initialize(argc,argv,version,build,error_buffer);
    if(!error)error=ProCmdActionAdd("BWTLImitationUiStart",imitation_ui_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLImitationUiTick",imitation_ui_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLImitationUiStart`;");
}
