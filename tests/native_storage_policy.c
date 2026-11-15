#define user_initialize production_storage_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int verify_storage_policy(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *receipt=fopen("storage-policy-native.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    ProPath workspace=L"",after=L"";
    int good_path=!ProDirectoryCurrentGet(workspace)&&storage_output_directory(workspace);
    const wchar_t drive=storage_output_drive();
    const wchar_t *other=drive==L'C'?L"E:\\":L"C:\\Windows";
    int switched=good_path&&!ProDirectoryChange((wchar_t *)other);
    wchar_t method[MAX_PATH];
    int method_store=method_directory(method,1)&&storage_output_directory(method);
    int denied=switched&&!storage_prepare_output();
    if(denied){
        smart_begin(L"新建测试零件");
        smart_readonly=0;
        pending_plan=(ModelPlan){0};
        pending_plan.ready=1;
        pending_plan.empty_kind=1;
        execute_model_plan();
        ProMdl active=NULL;
        denied=!pending_plan.ready&&ProMdlCurrentGet(&active)!=PRO_TK_NO_ERROR;
    }
    int restored=good_path&&!ProDirectoryChange(workspace)&&!ProDirectoryCurrentGet(after)&&
        !_wcsicmp(workspace,after)&&storage_prepare_output();
    ModelPlan plan={0};
    plan.a=20;plan.b=16;plan.height=12;plan.ready=1;
    ProMdlName name=L"";
    const char *stage=NULL;
    ProError error=restored?model_create(&plan,name,&stage):PRO_TK_BAD_CONTEXT;
    ProMdl model=NULL;
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
    if(!error)error=ProMdlSave(model);
    ProPath mask=L"";
    WIN32_FIND_DATAW found;
    HANDLE search=INVALID_HANDLE_VALUE;
    if(!error&&swprintf_s(mask,_countof(mask),L"%ls\\%ls.prt.2",workspace,name)>=0)
        search=FindFirstFileW(mask,&found);
    int saved=search!=INVALID_HANDLE_VALUE;
    if(saved)FindClose(search);
    fprintf(receipt,"DATA_DRIVE=%c\nOTHER_DRIVE_BLOCKED=%d\nWORKSPACE_RESTORED=%d\nSAVED_ON_SELECTED_DRIVE=%d\nMETHOD_STORE_ON_SELECTED_DRIVE=%d\nCREO=%d\nSTAGE=%s\nPASS=%d\n",
        (char)drive,denied,restored,saved,method_store,error,stage?stage:"none",denied&&restored&&saved&&method_store);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    SetEnvironmentVariableW(L"BWTL_OUTPUT_DRIVE",NULL);
    ProError error=production_storage_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLVerifyStorage",verify_storage_policy,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLVerifyStorage`;");
}
