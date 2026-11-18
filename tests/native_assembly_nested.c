#define user_initialize prior_assembly_initialize
#include "../src/assistant.c"
#undef user_initialize

static FILE *nested_log;
static int nested_failures;

static int nested_report(int good,const wchar_t *label,ProError error){
    if(!good)++nested_failures;
    if(nested_log){fwprintf(nested_log,L"%ls=%d Creo=%d\n",label,good,error);fflush(nested_log);}
    return good;
}

static ProError nested_native(ProMdl root,const wchar_t *line){
    PartCommand command;
    wchar_t receipt[4000]=L"";
    ProError error=part_command_parse(line,&command)==1?
        ax_execute(root,&command,receipt,_countof(receipt)):PRO_TK_BAD_INPUTS;
    if(nested_log){fwprintf(nested_log,L"%ls\n%ls",line,receipt);fflush(nested_log);}
    return error;
}

static int nested_label(ProMdl root,ProMdl model,wchar_t key[128]){
    AxScan *scan=NULL;
    if(ax_scan(root,1,&scan))return 0;
    int matches=0;
    for(int i=0;i<scan->count;i++)if(scan->occurrences[i].model==model){
        if(ax_path_label(scan->occurrences[i].ids,scan->occurrences[i].depth,key,128))++matches;
    }
    free(scan);
    return matches==1;
}

static ProError nested_csys(ProFeature *feature,ProError status,ProAppData context){
    if(status)return status;
    ProFeattype type;
    if(!ProFeatureTypeGet(feature,&type)&&type==PRO_FEAT_CSYS)
        ProModelitemNameGet(feature,context);
    return PRO_TK_NO_ERROR;
}

static int nested_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    const wchar_t *root_name=L"NESTED_FIXTURE_ROOT";
    wchar_t outer[128],inner[128],occurrence[128],line[512],local[128];
    ProMdl root=NULL,a=NULL,b=NULL,sheet=NULL,plain=NULL,stock=NULL,probe=NULL;
    ProMdlsubtype subtype;
    AxComponent found={0};
    ProMatrix pose;
    ProPath work;
    double mm;
    ProError error;
    nested_log=_wfopen(L"nested-probe.log",L"wt, ccs=UTF-8");
    if(!nested_log){ProEngineerEnd();return 0;}
#define MUST(step,rule) do{error=(step);if(!nested_report(!error,rule,error))goto end;}while(0)
#define CHECK(ok,rule) do{if(!nested_report(!!(ok),rule,0))goto end;}while(0)
#define OP(format,...) (swprintf_s(line,_countof(line),L"CREO_OP " format,__VA_ARGS__),nested_native(root,line))
    MUST(ProDirectoryCurrentGet(work),L"WORKSPACE");
    CHECK(towupper(work[0])==L'E',L"WORKSPACE_E");
    MUST(ax_new_subassembly(root_name,&root),L"ROOT_ASSEMBLY");
    MUST(qa_mm_per_unit(root,&mm),L"UNITS");
    MUST(OP(L"ASM_NEW_ASM %ls NESTED_FIXTURE_A DEFAULT",root_name),L"DEFAULT_SUBASSEMBLY");
    MUST(ProMdlnameInit(L"NESTED_FIXTURE_A",PRO_MDL_ASSEMBLY,&a),L"FIRST_MODEL");
    CHECK(nested_label(root,a,outer),L"FIRST_PATH");
    MUST(OP(L"ASM_NEW_ASM %ls NESTED_FIXTURE_B 18 7 2 0 0 90 IN %ls",root_name,outer),L"SECOND_LEVEL_COORDINATE_SUBASSEMBLY");
    MUST(ProMdlnameInit(L"NESTED_FIXTURE_B",PRO_MDL_ASSEMBLY,&b),L"SECOND_MODEL");
    CHECK(nested_label(root,b,inner),L"SECOND_LEVEL_PATH");
    MUST(ax_component_resolve(root,inner,&found),L"SECOND_LEVEL_RESOLVE");
    MUST(ax_actual_pose(a,&found.feature,pose),L"SECOND_LEVEL_POSE");
    CHECK(found.path.table_num==2&&fabs(pose[0][1]-1)<1e-7&&fabs(pose[3][0]*mm-18)<1e-5,L"SECOND_LEVEL_ACTUAL_POSITION");
    MUST(OP(L"ASM_NEW_PART %ls SAME SHEETMETAL IN %ls",root_name,outer),L"SAME_NUMBER_SHEET_METAL");
    MUST(ProMdlnameInit(L"NESTED_FIXTURE_A",PRO_MDL_PART,&sheet),L"DISTINCT_SAME_NAME_PART");
    MUST(ProMdlSubtypeGet(sheet,&subtype),L"SHEET_SUBTYPE_READBACK");
    CHECK(subtype==PROMDLSTYPE_PART_SHEETMETAL,L"SHEET_NATIVE_TYPE");
    MUST(template_empty((ProSolid)sheet),L"SHEET_TEMPLATE_EMPTY");
    CHECK(nested_label(root,sheet,occurrence),L"SHEET_PATH");
    MUST(ax_component_resolve(root,occurrence,&found),L"SHEET_RESOLVE");
    MUST(ax_default_verify(&found.feature),L"SHEET_DEFAULT_ACTIVE");
    MUST(OP(L"ASM_NEW_PART %ls NESTED_FIXTURE_SOLID SOLID IN %ls",root_name,inner),L"THIRD_LEVEL_SOLID");
    MUST(ProMdlnameInit(L"NESTED_FIXTURE_SOLID",PRO_MDL_PART,&plain),L"SOLID_MODEL");
    CHECK(nested_label(root,plain,occurrence),L"SOLID_PATH");
    MUST(ax_component_resolve(root,occurrence,&found),L"SOLID_RESOLVE");
    CHECK(found.path.table_num==3,L"SOLID_LEVEL");
    MUST(OP(L"ASM_PLACEMENT %ls %ls",root_name,occurrence),L"NESTED_CONSTRAINT_READ");
    CHECK(OP(L"ASM_NEW_PART %ls SAME SHEETMETAL IN %ls",root_name,outer)==PRO_TK_E_FOUND,L"EXACT_TYPE_DUPLICATE");

    ProMdlName stock_name=L"NESTED_FIXTURE_STOCK";
    const char *stage="nested-stock";
    ModelPlan stock_shape={0,24,16,5,0,1};
    MUST(quick_seed(stock_name,&stock),L"STOCK_MODEL");
    MUST(model_build_internal(&stock_shape,stock_name,&stage,stock,NULL,NULL),L"STOCK_GEOMETRY");
    MUST(OP(L"ASM_INSERT %ls %ls 30 15 12 0 0 90 FIX IN %ls",root_name,stock_name,inner),L"FIXED_COORDINATE_PART");
    CHECK(nested_label(root,stock,occurrence),L"FIXED_PATH");
    MUST(ax_component_resolve(root,occurrence,&found),L"FIXED_RESOLVE");
    MUST(ax_fixed_verify(&found.feature),L"FIXED_NATIVE_CONSTRAINT");
    MUST(ax_actual_pose(b,&found.feature,pose),L"FIXED_POSE");
    CHECK(found.path.table_num==3&&fabs(pose[0][1]-1)<1e-7&&fabs(pose[3][2]*mm-12)<1e-5,L"FIXED_ACTUAL_POSITION");
    MUST(OP(L"ASM_PLACEMENT %ls %ls",root_name,occurrence),L"NESTED_FIXED_READ");

    ProMdlName probe_name=L"NESTED_FIXTURE_CSYS";
    ProName base_csys=L"",part_csys=L"";
    MUST(quick_seed(probe_name,&probe),L"CSYS_MODEL");
    MUST(model_build_internal(&stock_shape,probe_name,&stage,probe,NULL,NULL),L"CSYS_GEOMETRY");
    MUST(OP(L"ASM_INSERT %ls %ls 8 3 2 IN %ls",root_name,probe_name,outer),L"TEMP_COORDINATE_PART");
    CHECK(nested_label(root,probe,occurrence),L"TEMP_PATH");
    MUST(ax_component_resolve(root,occurrence,&found),L"TEMP_RESOLVE");
    MUST(ax_packaged(&found.feature),L"TEMP_INCOMPLETE");
    CHECK(ax_path_label(&found.feature.id,1,local,_countof(local)),L"PARENT_LOCAL_ID");
    MUST(ProSolidFeatVisit((ProSolid)a,nested_csys,NULL,base_csys),L"PARENT_CSYS_NAME");
    MUST(ProSolidFeatVisit((ProSolid)probe,nested_csys,NULL,part_csys),L"PART_CSYS_NAME");
    CHECK(base_csys[0]&&part_csys[0],L"CSYS_BOTH_REAL");
    MUST(OP(L"ASM_CSYS %ls %ls CSYS:%ls CSYS:%ls IN %ls",root_name,local,base_csys,part_csys,outer),L"TEMP_TO_PERMANENT_CSYS");
    MUST(OP(L"ASM_PLACEMENT %ls %ls",root_name,occurrence),L"NESTED_CSYS_READ");
    ProMdl saved_only=NULL,library_only=NULL,library_module=NULL,loaded=NULL;
    ProMdlName saved_name=L"NESTED_DISK_STOCK",library_name=L"NESTED_LIBRARY_STOCK",module_name=L"NESTED_LIBRARY_ASM";
    int newly_loaded=0;
    MUST(ProConfigoptSet(L"override_store_back",L"yes"),L"LOCAL_SAVE_SETUP");
    MUST(quick_seed(saved_name,&saved_only),L"DISK_MODEL");
    MUST(model_build_internal(&stock_shape,saved_name,&stage,saved_only,NULL,NULL),L"DISK_GEOMETRY");
    MUST(ProMdlSave(saved_only),L"DISK_SAVE");
    MUST(ProMdlErase(saved_only),L"DISK_ERASE_FROM_SESSION");
    CHECK(ProMdlnameInit(saved_name,PRO_MDL_PART,&loaded)==PRO_TK_E_NOT_FOUND,L"DISK_NOT_LOADED");
    MUST(OP(L"ASM_INSERT %ls %ls DEFAULT IN %ls",root_name,saved_name,inner),L"DISK_PART_DEFAULT_INSERT");

    const wchar_t *folders[]={L"standard_fixture",L"standard_fixture\\data",L"standard_fixture\\公司资料",
        L"standard_fixture\\公司资料\\公司标准",L"standard_fixture\\公司资料\\公司标准\\公司标准件",
        L"standard_fixture\\公司资料\\公司标准\\公司标准件\\环境摄像头"};
    ProPath library;
    for(int i=0;i<_countof(folders);i++){
        CHECK(swprintf_s(library,_countof(library),L"%ls\\%ls",work,folders[i])>0,L"LIBRARY_PATH");
        CHECK(CreateDirectoryW(library,NULL)||GetLastError()==ERROR_ALREADY_EXISTS,L"LIBRARY_DIRECTORY");
    }
    CHECK(swprintf_s(ax_standard_data_directory,_countof(ax_standard_data_directory),L"%ls\\%ls",work,folders[1])>0,L"LIBRARY_DATA_ROOT");
    MUST(ProDirectoryChange(library),L"LIBRARY_WORKSPACE");
    MUST(quick_seed(library_name,&library_only),L"LIBRARY_PART_MODEL");
    MUST(model_build_internal(&stock_shape,library_name,&stage,library_only,NULL,NULL),L"LIBRARY_PART_GEOMETRY");
    MUST(ProMdlSave(library_only),L"LIBRARY_PART_SAVE");
    MUST(ax_new_subassembly(module_name,&library_module),L"LIBRARY_ASSEMBLY_MODEL");
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    wchar_t placing[512]=L"";
    PartText out={placing,_countof(placing),0};
    MUST(ax_place_model(library_module,library_only,identity,AX_PLACE_DEFAULT,mm,NULL,&out),L"LIBRARY_ASSEMBLY_POPULATED");
    MUST(ProMdlSave(library_module),L"LIBRARY_ASSEMBLY_SAVE");
    MUST(ProMdlErase(library_module),L"LIBRARY_ASSEMBLY_SESSION_ERASE");
    MUST(ProMdlErase(library_only),L"LIBRARY_PART_SESSION_ERASE");
    MUST(ProDirectoryChange(work),L"RETURN_WORKSPACE");
    CHECK(ProMdlnameInit(module_name,PRO_MDL_ASSEMBLY,&loaded)==PRO_TK_E_NOT_FOUND&&
          ProMdlnameInit(library_name,PRO_MDL_PART,&loaded)==PRO_TK_E_NOT_FOUND,L"LIBRARY_NOT_LOADED");
    MUST(OP(L"ASM_INSERT %ls %ls DEFAULT IN %ls",root_name,library_name,inner),L"EXISTING_LIBRARY_PART_INSERT");
    MUST(OP(L"ASM_INSERT_ASM %ls %ls DEFAULT IN %ls",root_name,module_name,inner),L"EXISTING_LIBRARY_ASSEMBLY_INSERT");
    MUST(ProMdlnameInit(module_name,PRO_MDL_ASSEMBLY,&loaded),L"EXISTING_LIBRARY_ASSEMBLY_REAL");
    CHECK(nested_label(root,loaded,occurrence),L"EXISTING_LIBRARY_ASSEMBLY_PATH");
    MUST(ax_component_resolve(root,occurrence,&found),L"EXISTING_LIBRARY_ASSEMBLY_RESOLVE");
    MUST(ax_default_verify(&found.feature),L"EXISTING_LIBRARY_ASSEMBLY_DEFAULT");
    CHECK(found.path.table_num==3,L"EXISTING_LIBRARY_ASSEMBLY_DEPTH");

    MUST(ax_load_native(library_name,PRO_MDL_PART,&loaded,&newly_loaded),L"LIBRARY_TYPED_PART_LOOKUP");
    CHECK(!ax_recovery_uncertain&&!part_health(root),L"ROOT_HEALTHY");
end:
    fwprintf(nested_log,L"NESTED_GENERIC_PASS=%d\n",!nested_failures);
    fclose(nested_log);
    ProEngineerEnd();
    return 0;
#undef MUST
#undef CHECK
#undef OP
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))
        return PRO_TK_BAD_CONTEXT;
    ProError error=prior_assembly_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLAssemblyNestedProbe",nested_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLAssemblyNestedProbe\x0060;");
}