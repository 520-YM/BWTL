#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize

static FILE *selected_log;
static int selected_failures;
static ProMdl selected_root,selected_parent;
static ProAsmcomp selected_feature;

static int chosen_result(int ok,const char *name,ProError error){
    if(!ok)++selected_failures;
    if(selected_log){fprintf(selected_log,"%s=%d Creo=%d\n",name,ok,error);fflush(selected_log);}
    return ok;
}

static ProError chosen_seed(void){
    ProMdlName name;
    const char *stage="empty assembly";
    model_background_mode=0;
    ProError error=model_create_empty(3,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&selected_root);
    if(!error)error=ax_new_subassembly(L"SELECTED_TEST_PARENT",&selected_parent);
    if(error)return error;
    ProMatrix identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    wchar_t scratch[256]=L"";
    PartText out={scratch,_countof(scratch),0};
    double mm;
    error=qa_mm_per_unit(selected_root,&mm);
    if(!error)error=ax_place_model(selected_root,selected_parent,identity,AX_PLACE_DEFAULT,mm,&selected_feature,&out);
    return error;
}

static int chosen_finish(void){
    if(selected_log){fprintf(selected_log,"SELECTED_PARENT_PASS=%d\n",!selected_failures);fclose(selected_log);}
    ProEngineerEnd();
    return 0;
}

static int chosen_start(uiCmdCmdId id,uiCmdValue *value,void *data){
    selected_log=fopen("selected-probe.log","wt");
    ProError error=chosen_seed();
    if(!chosen_result(!error,"SELECTED_FIXTURE",error))return chosen_finish();
    error=ProMacroLoad(L"~ Command \x0060" L"BWTLChosenVerify\x0060;");
    if(!chosen_result(!error,"SELECTED_FIXTURE_ACTIVE",error))return chosen_finish();
    return 0;
}

static int chosen_verify(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProError error;
    ProMdl current=NULL,child=NULL,sheet=NULL;
    ProMdlName name;
    ProMdlsubtype subtype;
    ProSelection selection=NULL;
    ProAsmcomppath path;
    ProIdTable direct={selected_feature.id};
    wchar_t key[AX_PATH_DEPTH*12+4],expected[AX_PATH_DEPTH*12+4];
    wchar_t reply[2400]=L"";
#define SELECTED_STEP(call,label) do{error=(call);if(!chosen_result(!error,label,error))goto done;}while(0)
#define SELECTED_CHECK(value,label) do{if(!chosen_result(!!(value),label,0))goto done;}while(0)
    SELECTED_STEP(ProMdlCurrentGet(&current),"SELECTION_ACTIVE_ROOT");
    SELECTED_CHECK(current==selected_root,"SELECTION_ROOT_MATCH");
    SELECTED_CHECK(assembly_selected_request(L"在选中的子装配里新建子装配 M26A0163001V00",name)==ASSEMBLY_SELECT_ASSEMBLY&&
        !wcscmp(name,L"M26A0163001V00"),"PARSE_SELECTED_ASSEMBLY");
    SELECTED_CHECK(assembly_selected_request(L"在选中的子装配里建同号钣金件",name)==ASSEMBLY_SELECT_SAME,"PARSE_SELECTED_SHEET");
    SELECTED_CHECK(assembly_selected_request(L"在选中的子装配里建空子零件",name)==ASSEMBLY_SELECT_SOLID,"PARSE_SELECTED_SOLID");
    SELECTED_STEP(ProSelbufferClear(),"CLEAR_ACTIVE_SELECTION");
    SELECTED_CHECK(assembly_selected_parent_path(selected_root,key)!=0,"REJECT_NO_SELECTION");
    SELECTED_STEP(ProAsmcomppathInit((ProSolid)selected_root,direct,1,&path),"SELECTION_PATH_BUILD");
    ProModelitem model_item;
    SELECTED_STEP(ProMdlToModelitem(selected_parent,&model_item),"SELECTION_MODEL_ITEM");
    SELECTED_STEP(ProSelectionAlloc(&path,&model_item,&selection),"NATIVE_SELECTION_MAKE");
    SELECTED_STEP(ProSelbufferSelectionAdd(selection),"NATIVE_SELECTION_BUFFER");
    SELECTED_STEP(assembly_selected_parent_path(selected_root,key),"SELECTION_EXACT_PARENT");
    SELECTED_CHECK(ax_path_label(direct,1,expected,_countof(expected))&&!wcscmp(key,expected),"SELECTION_PARENT_PATH_MATCH");
    SELECTED_STEP(assembly_selected_create(selected_root,key,ASSEMBLY_SELECT_ASSEMBLY,L"M26A0163001V00",reply,_countof(reply)),"SELECTED_NATIVE_ASSEMBLY_CREATED");
    SELECTED_STEP(ProMdlnameInit(L"M26A0163001V00",PRO_MDL_ASSEMBLY,&child),"SELECTED_CHILD_MODEL");
    SELECTED_STEP(assembly_selected_create(selected_root,key,ASSEMBLY_SELECT_SAME,L"",reply,_countof(reply)),"SELECTED_NATIVE_SHEET_CREATED");
    SELECTED_STEP(ProMdlnameInit(L"SELECTED_TEST_PARENT",PRO_MDL_PART,&sheet),"SELECTED_SAME_NUMBER");
    SELECTED_STEP(ProMdlSubtypeGet(sheet,&subtype),"SELECTED_SHEET_SUBTYPE");
    SELECTED_CHECK(subtype==PROMDLSTYPE_PART_SHEETMETAL,"SELECTED_SHEET_NATIVE");
    AxScan *scan=NULL;
    SELECTED_STEP(ax_scan(selected_parent,1,&scan),"SELECTED_PARENT_TREE");
    int two=scan->count==2;
    free(scan);
    SELECTED_CHECK(two,"BOTH_CREATED_UNDER_SELECTED_PARENT");
done:
    if(selection)ProSelectionFree(&selection);
    return chosen_finish();
#undef SELECTED_STEP
#undef SELECTED_CHECK
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))
        return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLChosenVerify",chosen_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLChosenStart",chosen_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLChosenStart\x0060;");
}