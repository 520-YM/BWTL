#define user_initialize bwtl_naming_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *naming_log;
static int naming_failures;
static ProPath naming_new_file,naming_original_file;
static ProMdlName naming_chat_number,naming_runtime_number=L"M26A1200000V00";
static const wchar_t naming_saved_old[]=L"BWTL_NAMING_SAVED";
static const wchar_t naming_saved_new[]=L"BWTL_NAMING_PUBLISHED";

static void naming_verify(int condition,const char *label){
    fprintf(naming_log,"%s=%d\n",label,!!condition);
    fflush(naming_log);
    naming_failures+=!condition;
}
static int naming_finish(void){
    fprintf(naming_log,"ASSEMBLY_NAMING_PASS=%d\n",naming_failures==0);
    fclose(naming_log);
    ProEngineerEnd();
    return 0;
}
static ProError naming_make(const wchar_t *requested,ProMdl *model){
    const char *stage=NULL;
    ProMdlName name=L"";
    int mode=model_background_mode;
    model_background_mode=1;
    ProError e=model_create_empty_named(3,requested,name,&stage);
    model_background_mode=mode;
    if(e)fprintf(naming_log,"CREATE_ERROR=%d STAGE=%s\n",e,stage);
    else e=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,model);
    return e;
}
static void naming_send(const wchar_t *text){
    native_prompt_set(text);
    run_local(NULL,NULL,NULL);
}
static ProError naming_rename(ProMdl model,const wchar_t *target){
    wchar_t message[1600]=L"";
    PartText result={message,_countof(message),0};
    ProError status=assembly_rename_apply(model,target,&result);
    if(status)fprintf(naming_log,"RENAME_STATUS=%d REPLY=%ls\n",status,message);
    return status;
}
static int naming_reopen_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL,original=NULL;
    ProMdlName current=L"";
    ProError missing=ProMdlnameInit((wchar_t *)naming_saved_new,PRO_MDL_ASSEMBLY,&model);
    naming_verify(missing==PRO_TK_E_NOT_FOUND,"TARGET_REMOVED_FROM_MEMORY_BEFORE_DISK_REOPEN");
    naming_verify(missing==PRO_TK_E_NOT_FOUND&&
        !ProMdlFiletypeLoad(naming_new_file,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&model)&&
        !ProMdlMdlnameGet(model,current)&&!_wcsicmp(current,naming_saved_new)&&!part_health(model),
        "RENAMED_SAVED_ASSEMBLY_REOPEN");
    naming_verify(!ProMdlFiletypeLoad(naming_original_file,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&original)&&
        !ProMdlMdlnameGet(original,current)&&!_wcsicmp(current,naming_saved_old)&&!part_health(original),
        "OLD_NAME_STILL_REOPENS_FOR_EXISTING_DISK_REFERENCES");
    return naming_finish();
}
static int naming_chat_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl active=NULL,lookup=NULL;
    ProMdlName name=L"";
    naming_verify(!ProMdlCurrentGet(&active)&&!ProMdlMdlnameGet(active,name)&&
        !_wcsicmp(name,naming_chat_number),"CHAT_CREATED_EXACT_COMPANY_ASSEMBLY_ACTIVE");
    if(naming_failures)return naming_finish();
    /* ProMacroLoad called by this fixture replaces the deferred verification
       loaded by the first operation. Run that production callback explicitly. */
    verify_active_command(0,NULL,NULL);
    naming_verify(!smart_active,"PRODUCTION_CREATE_BINDING_VERIFIED");
    naming_send(L"把当前装配改成 M26A1200000V00");
    naming_verify(!ProMdlnameInit(naming_runtime_number,PRO_MDL_ASSEMBLY,&lookup)&&lookup==active&&
        !ProMdlMdlnameGet(active,name)&&!_wcsicmp(name,naming_runtime_number),
        "CHAT_RENAMED_SAME_ASSEMBLY_HANDLE");
    if(naming_failures)return naming_finish();
    naming_send(L"在里面建同号钣金件");
    ProMdl sheet=NULL;
    ProMdlsubtype subtype;
    AxScan *scan=NULL;
    naming_verify(!ProMdlnameInit(naming_runtime_number,PRO_MDL_PART,&sheet)&&
        !ProMdlSubtypeGet(sheet,&subtype)&&subtype==PROMDLSTYPE_PART_SHEETMETAL&&
        !ax_scan(active,1,&scan)&&scan->count==1&&scan->occurrences[0].model==sheet,
        "FOLLOW_UP_SAME_NUMBER_NATIVE_SHEET_ON_RENAMED_ASSEMBLY");
    free(scan);

    ProMdl existing_part=NULL,coexist_asm=NULL;
    ProMdlName coexist=L"BWTL_NAMING_COEXIST";
    naming_verify(!quick_seed(coexist,&existing_part)&&!naming_make(coexist,&coexist_asm)&&coexist_asm!=existing_part,
        "EXISTING_SAME_NAME_PART_DOES_NOT_BLOCK_ASSEMBLY");
    naming_verify(naming_make(coexist,&lookup)==PRO_TK_E_FOUND,"EXISTING_ASSEMBLY_NAME_BLOCKED");
    ProMdl child=NULL,parent=NULL;
    ProAsmcomp occurrence={0};
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProError stage=naming_make(L"BWTL_NAMING_REFERENCED",&child);
    if(!stage)stage=naming_make(L"BWTL_NAMING_PARENT",&parent);
    if(!stage)stage=ProAsmcompAssemble((ProAssembly)parent,(ProSolid)child,pose,&occurrence);
    naming_verify(!stage,"LOADED_PARENT_FIXTURE");
    if(!stage)naming_verify(naming_rename(child,L"BWTL_NAMING_NO_PARENT")==PRO_TK_E_IN_USE,
        "LOADED_PARENT_RENAME_PROTECTED");

    ProMdl drawing_target=NULL;
    ProDrawing drawing=NULL;
    ProDwgcreateErrs diagnostics=NULL;
    ProMdlName generated_drawing=L"BWTL_NAMING_DRAWING",drawing_template=L"a4_drawing";
    ProMdlnameShortdata drawing_ref={0};
    wcscpy_s(drawing_ref.name,_countof(drawing_ref.name),L"BWTL_NAMING_DRAW_TARGET");
    wcscpy_s(drawing_ref.type,_countof(drawing_ref.type),L"asm");
    stage=naming_make(drawing_ref.name,&drawing_target);
    if(!stage)stage=ProDrawingFromTemplateCreate(generated_drawing,drawing_template,&drawing_ref,0,&drawing,&diagnostics);
    if(diagnostics)ProDwgcreateErrsFree(&diagnostics);
    if(stage)fprintf(naming_log,"DRAWING_FIXTURE_STATUS=%d\n",stage);
    naming_verify(!stage&&drawing,"LOADED_DRAWING_FIXTURE");
    if(!stage)naming_verify(naming_rename(drawing_target,L"BWTL_NAMING_NO_DRW")==PRO_TK_E_IN_USE,
        "LOADED_DRAWING_RENAME_PROTECTED");

    ProPath directory;
    ProMdl saved=NULL;
    int occupied;
    stage=ProDirectoryCurrentGet(directory);
    if(!stage)stage=naming_make(naming_saved_old,&saved);
    if(!stage)stage=ProMdlSave(saved);
    if(!stage)stage=model_assembly_disk(directory,naming_saved_old,naming_original_file,&occupied);
    naming_verify(!stage&&naming_original_file[0],"SAVED_ASSEMBLY_FIXTURE");
    if(!stage)stage=naming_rename(saved,naming_saved_new);
    if(!stage)stage=model_assembly_disk(directory,naming_saved_new,naming_new_file,&occupied);
    naming_verify(!stage&&naming_new_file[0]&&dv_file_nonempty(naming_original_file),
        "SAVED_RENAME_WRITES_NEW_PATH_AND_PRESERVES_ORIGINAL");
    if(stage||!naming_new_file[0])return naming_finish();
    stage=ProMdlErase(saved);
    naming_verify(!stage,"PROBE_MODEL_ERASED_BEFORE_DISK_REOPEN");
    if(stage||ProMacroLoad(L"~ Command \x0060" L"BWTLNamingReopen\x0060;")){
        naming_failures++;
        return naming_finish();
    }
    return 0;
}
static int naming_start(uiCmdCmdId id,uiCmdValue *value,void *data){
    naming_log=fopen("assembly-naming-native.log","w");
    if(!naming_log){ProEngineerEnd();return 0;}
    ProMdlName parsed=L"";
    SYSTEMTIME today;
    GetLocalTime(&today);
    swprintf_s(naming_chat_number,PRO_NAME_SIZE,L"M%02uA0100000V00",today.wYear%100);
    naming_verify(assembly_naming_request(L"创建总装，图号01",parsed)==ASSEMBLY_NAMING_CREATE&&
        !_wcsicmp(parsed,naming_chat_number),"CHAT_COMPANY_RULE_AND_VERSION");
    naming_verify(assembly_naming_request(L"创建 M26A1500000V00 总装",parsed)==ASSEMBLY_NAMING_CREATE&&
        !_wcsicmp(parsed,L"M26A1500000V00"),"CHAT_EXACT_FULL_DIAGRAM_NUMBER");
    ModelPlan plan={0};
    wchar_t serialized[256];
    int accepted=model_plan_parse(L"CREO_PLAN EMPTY_ASSEMBLY M26A1300000V00",&plan);
    model_plan_format(&plan,serialized);
    naming_verify(accepted&&!wcscmp(serialized,L"CREO_PLAN EMPTY_ASSEMBLY M26A1300000V00"),
        "NAMED_ASSEMBLY_PROTOCOL_ROUNDTRIP");
    naming_verify(!open_window(0,NULL,NULL)&&native_window,"ISOLATED_BWTL_CHAT_WINDOW");
    if(naming_failures)return naming_finish();
    naming_send(L"创建总装，图号01");
    if(ProMacroLoad(L"~ Command \x0060" L"BWTLNamingProbe\x0060;")){
        naming_failures++;
        return naming_finish();
    }
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_naming_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId cmd;
    if(!error)error=ProCmdActionAdd("BWTLNamingStart",naming_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLNamingProbe",naming_chat_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLNamingReopen",naming_reopen_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLNamingStart\x0060;");
}