#define user_initialize component_open_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

#define OPEN_TEST_MODELS 5
#define OPEN_TEST_CASES 8

typedef struct {
    ProMdl model;
    ProWVerstamp stamp;
    ProBoolean dirty;
} OpenTestModel;

typedef struct {
    wchar_t protocol[512];
    ProMdl expected;
    const wchar_t *label;
    int denied;
} OpenTestCase;

static const wchar_t open_root_name[]=L"OPEN_FIXTURE_TOP";
static const wchar_t open_outer_name[]=L"OPEN_FIXTURE_OUTER";
static const wchar_t open_inner_name[]=L"OPEN_FIXTURE_INNER";
static const wchar_t open_solid_name[]=L"OPEN_FIXTURE_SOLID";
static ProMdl open_root,open_sheet,open_outer,open_inner,open_solid;
static OpenTestModel open_states[OPEN_TEST_MODELS];
static OpenTestCase open_cases[OPEN_TEST_CASES];
static ProPath open_directory;
static FILE *open_result;
static wchar_t selected_solid_path[128];
static int open_root_window,open_sheet_window;
static int open_failures,open_case_index,open_check_pending;
static unsigned open_file_count;
static ULONGLONG open_file_digest;

static int open_check(const wchar_t *label,int pass,ProError error){
    if(!pass)open_failures++;
    fwprintf(open_result,L"%ls=%d Creo=%d\n",label,!!pass,error);
    fflush(open_result);
    return pass;
}

static int open_finish(void){
    for(int i=0;i<OPEN_TEST_MODELS;i++)
        if(open_states[i].stamp)ProVerstampFree(&open_states[i].stamp);
    if(open_result){
        fwprintf(open_result,L"COMPONENT_OPEN_NATIVE_PASS=%d\n",!open_failures);
        fclose(open_result);
        open_result=NULL;
    }
    ProEngineerEnd();
    return 0;
}

static ULONGLONG open_disk_state(const wchar_t *directory,unsigned *count){
    const wchar_t *patterns[]={L"*.asm*",L"*.prt*"};
    ULONGLONG digest=0;
    *count=0;
    for(int i=0;i<_countof(patterns);i++){
        wchar_t glob[PRO_PATH_SIZE];
        if(swprintf_s(glob,_countof(glob),L"%ls\\%ls",directory,patterns[i])<0)return 0;
        WIN32_FIND_DATAW entry;
        HANDLE find=FindFirstFileW(glob,&entry);
        if(find==INVALID_HANDLE_VALUE)continue;
        do{
            if(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
            ULONGLONG stamp=14695981039346656037ULL;
            for(const wchar_t *p=entry.cFileName;*p;p++){
                stamp^=*p;
                stamp*=1099511628211ULL;
            }
            stamp^=((ULONGLONG)entry.nFileSizeHigh<<32)|entry.nFileSizeLow;
            stamp*=1099511628211ULL;
            stamp^=((ULONGLONG)entry.ftLastWriteTime.dwHighDateTime<<32)|entry.ftLastWriteTime.dwLowDateTime;
            digest^=stamp;
            ++*count;
        }while(FindNextFileW(find,&entry));
        FindClose(find);
    }
    return digest;
}

static int open_model_state_save(void){
    ProMdl models[]={open_root,open_sheet,open_outer,open_inner,open_solid};
    for(int i=0;i<_countof(models);i++){
        OpenTestModel *current=&open_states[i];
        current->model=models[i];
        if(ProMdlVerstampGet(current->model,&current->stamp)||
           ProMdlModificationVerify(current->model,&current->dirty))return 0;
    }
    if(ProDirectoryCurrentGet(open_directory))return 0;
    open_file_digest=open_disk_state(open_directory,&open_file_count);
    return 1;
}

static int open_model_state_same(void){
    ProPath directory;
    unsigned count;
    if(ProDirectoryCurrentGet(directory)||_wcsicmp(directory,open_directory)||
       open_disk_state(directory,&count)!=open_file_digest||count!=open_file_count)return 0;
    for(int i=0;i<OPEN_TEST_MODELS;i++){
        OpenTestModel *current=&open_states[i];
        ProWVerstamp stamp=NULL;
        ProBoolean dirty=PRO_B_FALSE;
        const int same=!ProMdlVerstampGet(current->model,&stamp)&&
            ProVerstampEqual(current->stamp,stamp)&&
            !ProMdlModificationVerify(current->model,&dirty)&&dirty==current->dirty;
        if(stamp)ProVerstampFree(&stamp);
        if(!same){
            fwprintf(open_result,L"MODEL_STATE_CHANGED index=%d\n",i);
            return 0;
        }
    }
    return 1;
}

static ProError open_execute(const wchar_t *line){
    PartCommand command;
    PartSnapshot snapshot={0};
    wchar_t report[PART_TEXT_MAX]=L"";
    ProError error=part_snapshot_take_mode(open_root,&snapshot,0);
    if(!error&&part_command_parse(line,&command)!=1)error=PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&command,&snapshot,report,_countof(report));
    part_snapshot_clear(&snapshot);
    fwprintf(open_result,L"%ls -> Creo %d\n%ls\n",line,error,report);
    fflush(open_result);
    return error;
}

static int open_paths(ProMdl model,int depth,wchar_t first[128],wchar_t *second){
    AxScan *scan=NULL;
    if(ax_scan(open_root,1,&scan))return 0;
    int count=0;
    for(int i=0;i<scan->count;i++){
        const AxOccurrence *occurrence=&scan->occurrences[i];
        if(occurrence->model!=model||occurrence->depth!=depth)continue;
        wchar_t *target=count==0?first:count==1?second:NULL;
        if(target&&!ax_path_label(occurrence->ids,occurrence->depth,target,128)){
            count=0;
            break;
        }
        ++count;
    }
    free(scan);
    return count;
}

static int open_request(const wchar_t *macro){
    ProError error=ProMacroLoad((wchar_t *)macro);
    return open_check(L"QUEUE_NEXT_CREO_COMMAND",!error,error);
}

static void open_set_case(int index,const wchar_t *verb,const wchar_t *target,
                          ProMdl model,const wchar_t *label,int denied){
    OpenTestCase *test=&open_cases[index];
    swprintf_s(test->protocol,_countof(test->protocol),L"CREO_OP %ls %ls %ls",verb,open_root_name,target);
    test->expected=model;
    test->label=label;
    test->denied=denied;
}

static ProError open_select_occurrence(const wchar_t *occurrence,ProMdl model){
    ProIdTable ids={0};
    int depth=ax_path_parse(occurrence,ids);
    if(!depth)return PRO_TK_BAD_INPUTS;
    ProAsmcomppath path;
    ProModelitem item;
    ProSelection choice=NULL;
    ProError error=ProSelbufferClear();
    if(!error)error=ProAsmcomppathInit((ProSolid)open_root,ids,depth,&path);
    if(!error)error=ProMdlToModelitem(model,&item);
    if(!error)error=ProSelectionAlloc(&path,&item,&choice);
    if(!error)error=ProSelbufferSelectionAdd(choice);
    if(choice)ProSelectionFree(&choice);
    return error;
}

static int open_step(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active=NULL;
    int current_window=0,model_window=0;
    ProError error=ProMdlCurrentGet(&active);
    if(!open_check(L"READ_CURRENT_MODEL",!error,error))return open_finish();

    if(open_check_pending){
        OpenTestCase *test=&open_cases[open_case_index];
        int active_ok=active==test->expected&&!ProWindowCurrentGet(&current_window)&&
            !ProMdlWindowGet(active,&model_window)&&current_window==model_window;
        if(!test->denied){
            wchar_t receipt[1800];
            ProError readback=ax_open_readback(receipt,_countof(receipt));
            fwprintf(open_result,L"%ls\n",receipt);
            active_ok=active_ok&&!readback;
        }else if(ax_open_pending.target)active_ok=0;
        if(open_case_index==0){
            ProMdlsubtype subtype;
            active_ok=active_ok&&!ProMdlSubtypeGet(active,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL;
            if(active_ok)open_sheet_window=model_window;
        }else if(open_case_index==4){
            active_ok=active_ok&&model_window==open_sheet_window;
        }
        if(!open_check(test->label,active_ok,0)||
           !open_check(L"WINDOW_SWITCH_PRESERVES_MODELS_AND_FILES",open_model_state_same(),0))
            return open_finish();

        if(active!=open_root){
            error=ProWindowActivate(open_root_window);
            if(!open_check(L"RETURN_TO_PARENT_EXISTING_WINDOW",!error,error))return open_finish();
        }
        open_check_pending=0;
        open_case_index++;
    }else{
        if(!open_check(L"PARENT_IS_ACTIVE_BEFORE_SELECTION",active==open_root,0))return open_finish();
        if(open_case_index==OPEN_TEST_CASES){
            open_check(L"PARENT_ASSEMBLY_HEALTH_AND_DISK_STATE",
                !part_health(open_root)&&open_model_state_same(),0);
            return open_finish();
        }
        OpenTestCase *test=&open_cases[open_case_index];
        if(open_case_index==7){
            error=open_select_occurrence(selected_solid_path,open_solid);
            if(!open_check(L"SEED_NATIVE_SELECTION_OF_NESTED_PART",!error,error))return open_finish();
        }
        error=open_execute(test->protocol);
        if(!open_check(L"PROTOCOL_OUTCOME",test->denied?error!=PRO_TK_NO_ERROR:error==PRO_TK_NO_ERROR,error))
            return open_finish();
        open_check_pending=1;
    }
    if(!open_request(L"~ Command \x0060" L"BWTLComponentOpenStep\x0060;"))return open_finish();
    return 0;
}

static int open_prepare(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active=NULL;
    wchar_t direct[128],repeat[128],outer[128],inner[128],solid[128],line[512];
    ProError error=ProMdlCurrentGet(&active);
    if(!open_check(L"FIXTURE_ASSEMBLY_WINDOW",!error&&active==open_root,error))return open_finish();
#define OPEN_SETUP(label,expr) do{error=(expr);if(!open_check(L##label,!error,error))return open_finish();}while(0)
    OPEN_SETUP("PARENT_WINDOW_ID",ProMdlWindowGet(open_root,&open_root_window));
    swprintf_s(line,_countof(line),L"CREO_OP ASM_NEW_PART %ls SAME SHEETMETAL DEFAULT",open_root_name);
    OPEN_SETUP("CREATE_SAME_STEM_SHEET_METAL",open_execute(line));
    OPEN_SETUP("FIND_SAME_STEM_PART",ProMdlnameInit((wchar_t *)open_root_name,PRO_MDL_PART,&open_sheet));
    swprintf_s(line,_countof(line),L"CREO_OP ASM_NEW_ASM %ls %ls DEFAULT",open_root_name,open_outer_name);
    OPEN_SETUP("CREATE_DIRECT_SUBASSEMBLY",open_execute(line));
    OPEN_SETUP("FIND_DIRECT_SUBASSEMBLY",ProMdlnameInit((wchar_t *)open_outer_name,PRO_MDL_ASSEMBLY,&open_outer));
    if(!open_check(L"DIRECT_SUBASSEMBLY_PATH",open_paths(open_outer,1,outer,NULL)==1,0))return open_finish();
    swprintf_s(line,_countof(line),L"CREO_OP ASM_NEW_ASM %ls %ls DEFAULT IN %ls",open_root_name,open_inner_name,outer);
    OPEN_SETUP("CREATE_NESTED_SUBASSEMBLY",open_execute(line));
    OPEN_SETUP("FIND_NESTED_SUBASSEMBLY",ProMdlnameInit((wchar_t *)open_inner_name,PRO_MDL_ASSEMBLY,&open_inner));
    if(!open_check(L"NESTED_SUBASSEMBLY_PATH",open_paths(open_inner,2,inner,NULL)==1,0))return open_finish();
    swprintf_s(line,_countof(line),L"CREO_OP ASM_NEW_PART %ls %ls SOLID DEFAULT IN %ls",open_root_name,open_solid_name,inner);
    OPEN_SETUP("CREATE_THIRD_LEVEL_SOLID",open_execute(line));
    OPEN_SETUP("FIND_THIRD_LEVEL_SOLID",ProMdlnameInit((wchar_t *)open_solid_name,PRO_MDL_PART,&open_solid));
    if(!open_check(L"THIRD_LEVEL_SOLID_PATH",open_paths(open_solid,3,solid,NULL)==1,0))return open_finish();

    ProMatrix placement={{1,0,0,0},{0,1,0,0},{0,0,1,0},{12,0,0,1}};
    wchar_t report[2000]=L"";
    PartText out={report,_countof(report),0};
    double mm=0;
    OPEN_SETUP("ROOT_UNITS",qa_mm_per_unit(open_root,&mm));
    OPEN_SETUP("SECOND_OCCURRENCE_OF_SAME_SHEET",
        ax_place_model(open_root,open_sheet,placement,AX_PLACE_FIXED,mm,NULL,&out));
    fwprintf(open_result,L"%ls\n",report);
    if(!open_check(L"DISTINCT_OCCURRENCE_PATHS",
        open_paths(open_sheet,1,direct,repeat)==2&&wcscmp(direct,repeat),0))return open_finish();

    open_set_case(0,L"ASM_OPEN",direct,open_sheet,L"OPENS_SAME_STEM_SHEET_METAL_IN_PART_WINDOW",0);
    open_set_case(1,L"ASM_OPEN",inner,open_inner,L"OPENS_NESTED_SUBASSEMBLY_IN_ASSEMBLY_WINDOW",0);
    open_set_case(2,L"ASM_OPEN",solid,open_solid,L"OPENS_THIRD_LEVEL_SOLID_IN_PART_WINDOW",0);
    open_set_case(3,L"DISPLAY",open_outer_name,open_outer,L"LEGACY_DISPLAY_OPENS_UNIQUE_SUBASSEMBLY",0);
    open_set_case(4,L"ASM_OPEN",repeat,open_sheet,L"SECOND_OCCURRENCE_REUSES_SHARED_MODEL_WINDOW",0);
    open_set_case(5,L"DISPLAY",open_root_name,open_root,L"AMBIGUOUS_NAME_LEAVES_PARENT_ACTIVE",1);
    open_set_case(6,L"ASM_OPEN",L"ID:2147483647",open_root,L"UNKNOWN_OCCURRENCE_LEAVES_PARENT_ACTIVE",1);
    wcscpy_s(selected_solid_path,_countof(selected_solid_path),solid);
    open_set_case(7,L"ASM_OPEN",L"SELECTED",open_solid,L"SELECTED_NESTED_PART_OPENS_IN_ITS_WINDOW",0);
    if(!open_request(L"~ Command \x0060" L"BWTLComponentOpenCapture\x0060;"))return open_finish();
    return 0;
#undef OPEN_SETUP
}

static int open_capture(uiCmdCmdId command,uiCmdValue *value,void *data){
    if(!open_check(L"CAPTURE_POST_SETUP_MODEL_AND_DISK_STATE",open_model_state_save(),0)||
       !open_request(L"~ Command \x0060" L"BWTLComponentOpenStep\x0060;"))return open_finish();
    return 0;
}

static int open_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    const char *stage=NULL;
    ProMdlName name;
    open_result=_wfopen(L"component-open-native.log",L"wt, ccs=UTF-8");
    if(!open_result){ProEngineerEnd();return 0;}
    if(!open_check(L"ISOLATED_E_WORKSPACE",storage_prepare_output(),0))return open_finish();
    ProError error=model_create_empty_named(3,open_root_name,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&open_root);
    if(!open_check(L"CREATE_INDEPENDENT_ROOT",!error,error)){
        fwprintf(open_result,L"setup stage %hs\n",stage?stage:"unknown");
        return open_finish();
    }
    if(!open_request(L"~ Command \x0060" L"BWTLComponentOpenPrepare\x0060;"))return open_finish();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=component_open_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId cmd;
    if(!error)error=ProCmdActionAdd("BWTLComponentOpenBegin",open_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLComponentOpenPrepare",open_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLComponentOpenCapture",open_capture,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLComponentOpenStep",open_step,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLComponentOpenBegin\x0060;");
}
