#define user_initialize attachment_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *receipt;
static int failures;
static ProPath part_fixture,assembly_fixture,step_fixture;
static void check(int pass,const char *label){
    if(!pass)failures++;
    fprintf(receipt,"%s=%d\n",label,!!pass);
    fflush(receipt);
}

static ProError make_files(ProPath directory,ProPath part_path,ProPath asm_path,ProPath step_path){
    ModelPlan plan={0};
    ProMdlName part_name,asm_name;
    ProMdl part=NULL,assembly=NULL;
    ProPath extension=L"";
    ProAsmcomp component;
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const char *stage=NULL;
    plan.a=27;plan.b=18;plan.height=13;plan.ready=1;
    model_background_mode=1;
    ProError error=model_create(&plan,part_name,&stage);
    if(!error)error=ProMdlnameInit(part_name,PRO_MDL_PART,&part);
    if(!error)error=ProMdlSave(part);
    if(!error)error=ProMdlOriginGet(part,part_path);
    if(!error)error=dv_export_file(part,directory,PRO_INTF_EXPORT_STEP,extension);
    if(!error&&swprintf_s(step_path,PRO_PATH_SIZE,L"%ls\\%ls",directory,extension)<0)error=PRO_TK_BAD_INPUTS;
    if(!error)error=model_create_empty(3,asm_name,&stage);
    if(!error)error=ProMdlnameInit(asm_name,PRO_MDL_ASSEMBLY,&assembly);
    if(!error)error=ProAsmcompAssemble((ProAssembly)assembly,(ProSolid)part,pose,&component);
    if(!error)error=ProMdlSave(assembly);
    if(!error)error=ProMdlOriginGet(assembly,asm_path);
    model_background_mode=0;
    if(assembly)ProMdlErase(assembly);
    if(part)ProMdlErase(part);
    return error;
}
static int test_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    static NativeFile files[NATIVE_ATTACHMENT_MAX];
    wchar_t error[256],report[12000];
    ProMdl before=NULL,after=NULL;
    ProPath directory,restored;
    if(!receipt)return 0;
    ProMdlCurrentGet(&before);
    check(before==NULL,"STARTS_WITH_NO_ACTIVE_MODEL");
    check(!ProDirectoryCurrentGet(directory),"CAPTURE_WORK_DIRECTORY");
    {
        check(native_attachment_import(part_fixture,error,_countof(error)),"PRT_REVISION_ATTACHES");
        check(native_attachment_import(assembly_fixture,error,_countof(error)),"ASM_REVISION_ATTACHES");
        check(native_attachment_import(step_fixture,error,_countof(error)),"STEP_ATTACHES");
        check(native_attachment_count()==3,"MIXED_ATTACHMENTS");
        if(native_files_snapshot(files)==3){
            const wchar_t *details_path[]={L"attachment-prt-details.txt",L"attachment-asm-details.txt",L"attachment-step-details.txt"};
            const wchar_t *expected[]={L"外形范围",L"组件实例 1",L"外形范围"};
            const char *labels[]={"PRT_DIMENSIONS_WITH_NO_ACTIVE_MODEL","ASM_COMPONENT_TREE","STEP_GEOMETRY_READBACK"};
            for(int i=0;i<3;i++){
                ProError status=attachment_cad_report(&files[i],report,_countof(report));
                check(!status&&wcsstr(report,expected[i])!=NULL,labels[i]);
                fprintf(receipt,"CAD_%d_CODE=%d\n",i,status);
                FILE *details=_wfopen(details_path[i],L"wt, ccs=UTF-8");
                if(details){fputws(report,details);fclose(details);}
            }
        }
    }
    attachment_cleanup_command(0,NULL,NULL);
    SendMessageW(native_window,CHAT_ATTACHMENTS_SENT,0,0);
    check(!native_attachment_count()&&GetDlgItem(native_window,CHAT_ATTACH)&&GetDlgItem(native_window,CHAT_CLEAR_IMAGES),"ATTACHMENT_CONTROLS_AND_RESET");
    ProMdlCurrentGet(&after);
    check(before==after,"ACTIVE_MODEL_PRESERVED");
    check(!ProDirectoryCurrentGet(restored)&&!_wcsicmp(directory,restored),"WORK_DIRECTORY_PRESERVED");
    fprintf(receipt,"BEFORE=%p AFTER=%p\n",before,after);
    fprintf(receipt,"DIR_BEFORE=%ls DIR_AFTER=%ls\n",directory,restored);
    fprintf(receipt,"CHAT_FILE_ANALYSIS_NATIVE_PASS=%d\n",!failures);
    fclose(receipt);
    ProEngineerEnd();return 0;
}
static int prepare_command(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProPath directory;
    receipt=fopen("chat-file-analysis-native.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    ProError error=ProDirectoryCurrentGet(directory);
    check(!error&&open_window(0,NULL,NULL)==0,"PRIVATE_CREO_AND_CHAT");
    if(!error)error=make_files(directory,part_fixture,assembly_fixture,step_fixture);
    check(!error,"SYNTHETIC_PART_ASSEMBLY_AND_STEP_SAVED");
    fprintf(receipt,"FIXTURE_CODE=%d\n",error);
    if(!error)error=ProMacroLoad(L"~ Command \x0060" L"BWTLAttachmentCheck\x0060;");
    if(error){
        fprintf(receipt,"CHAT_FILE_ANALYSIS_NATIVE_PASS=0\n");
        fclose(receipt);
        ProEngineerEnd();
    }
    return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=attachment_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLAttachmentCheck",test_command,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(!error)error=ProCmdActionAdd("BWTLAttachmentPrepare",prepare_command,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLAttachmentPrepare\x0060;");
}
