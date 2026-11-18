#define user_initialize company_design_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static FILE *receipt;
static int failures;
static ProPath work_dir,project_dir,library_dir,library_file,workspace_file;
static ProMdlName library_name,assembly_name;
static ProMdl assembly;
static ProWVerstamp library_model_stamp,placed_model_stamp;
static wchar_t model_report[30000];

static void check(int pass,const char *label){
    if(!pass)failures++;
    fprintf(receipt,"%s=%d\n",label,!!pass);
    fflush(receipt);
}

static void finish(void){
    if(library_model_stamp)ProVerstampFree(&library_model_stamp);
    if(placed_model_stamp)ProVerstampFree(&placed_model_stamp);
    fprintf(receipt,"COMPANY_DESIGN_NATIVE_PASS=%d\n",!failures);
    fclose(receipt);
    ProEngineerEnd();
}

static void dump_report(const wchar_t *name,const wchar_t *report){
    FILE *log=_wfopen(name,L"a, ccs=UTF-8");
    if(log){fputws(report,log);fputws(L"\n",log);fclose(log);}
}

static ProError scoped_library_setup(void){
    ProPath bucket,nested;
    ProError error=ProDirectoryCurrentGet(work_dir);
    if(error)return error;
    company_trim_path(work_dir);
    if(swprintf_s(library_dir,_countof(library_dir),L"%ls\\catalog-allowed",work_dir)<0||
       swprintf_s(bucket,_countof(bucket),L"%ls\\hardware",library_dir)<0||
       swprintf_s(nested,_countof(nested),L"%ls\\machined",bucket)<0||
       !CreateDirectoryW(library_dir,NULL)||!CreateDirectoryW(bucket,NULL)||!CreateDirectoryW(nested,NULL))
        return PRO_TK_CANT_WRITE;
    const char *stage=NULL;
    ProMdl part=NULL;
    ModelPlan plan={0};
    plan.a=50;plan.b=30;plan.height=6;plan.hole=8;plan.ready=1;
    model_background_mode=1;
    error=model_create(&plan,library_name,&stage);
    model_background_mode=0;
    if(!error)error=ProMdlnameInit(library_name,PRO_MDL_PART,&part);
    if(!error)error=ProMdlSave(part);
    if(!error)error=ProMdlOriginGet(part,workspace_file);
    if(!error){
        const wchar_t *leaf=wcsrchr(workspace_file,L'\\');
        if(!leaf||swprintf_s(library_file,_countof(library_file),L"%ls%ls",nested,leaf)<0||
           !CopyFileW(workspace_file,library_file,TRUE))error=PRO_TK_CANT_WRITE;
    }
    if(part)ProMdlErase(part);
    return error;
}

static int design_setup(uiCmdCmdId command,uiCmdValue *value,void *data){
    receipt=fopen("company-design-native.log","w");
    if(!receipt){ProEngineerEnd();return 0;}
    ProError error=scoped_library_setup();
    check(!error,"SYNTHETIC_PART_SAVED_AND_COPIED_TO_NESTED_LIBRARY");
    fprintf(receipt,"FIXTURE_CODE=%d\n",error);
    if(!error)error=ProMacroLoad(L"~ Command \x0060" L"BWTLCompanyDesignLibrary\x0060;");
    if(error){check(0,"NEXT_LIBRARY_PHASE");finish();}
    return 0;
}

static int test_project_path(void){
    ProPath duplicate,actual;
    wchar_t operation[MAX_PATH+16],response[1000];
    const wchar_t *leaf=wcsrchr(workspace_file,L'\\');
    if(!leaf||swprintf_s(project_dir,_countof(project_dir),L"%ls\\project-output",work_dir)<0||
       !CreateDirectoryW(project_dir,NULL)||
       swprintf_s(duplicate,_countof(duplicate),L"%ls%ls",project_dir,leaf)<0||
       !CopyFileW(workspace_file,duplicate,TRUE))return 0;
    swprintf_s(smart_latest_request,_countof(smart_latest_request),
        L"请把工作目录切换到「%ls」，之后保存项目。",project_dir);
    swprintf_s(operation,_countof(operation),L"workdir|%ls|",work_dir);
    resident_tool_inflight=1;
    company_project_dispatch(operation);
    wchar_t *body=NULL;
    int rejected=resident_read(L"resident-tool-result.txt",response,_countof(response))==1&&
        resident_frame(response,&body)&&body&&wcsncmp(body,L"0\n",2)!=0;
    check(rejected,"PROJECT_DISPATCH_REJECTS_UNREQUESTED_PATH");
    swprintf_s(operation,_countof(operation),L"workdir|%ls|",project_dir);
    resident_tool_inflight=1;
    company_project_dispatch(operation);
    body=NULL;
    int passed=resident_read(L"resident-tool-result.txt",response,_countof(response))==1&&
        resident_frame(response,&body)&&body&&!wcsncmp(body,L"0\n",2)&&!ProDirectoryCurrentGet(actual);
    if(!passed)return 0;
    company_trim_path(actual);
    return !_wcsicmp(actual,project_dir);
}

static int test_library(uiCmdCmdId command,uiCmdValue *value,void *data){
    wchar_t listings[10000]=L"",verified[MAX_PATH];
    PartText out={listings,_countof(listings),0};
    swprintf_s(smart_latest_request,_countof(smart_latest_request),L"查找标准件目录「%ls」",library_dir);
    company_root_count=0;
    company_source_reset();
    ProError status=company_list(L"E:\\BWTL-Data",L"",0,&out);
    check(status!=PRO_TK_NO_ERROR,"OUTSIDE_DIRECTORY_REJECTED");
    listings[0]=0;out.truncated=0;
    status=company_list(library_dir,L"",0,&out);
    check(!status&&wcsstr(listings,L"hardware")&&!wcsstr(listings,L"machined"),"DIRECTORY_LIST_RETURNS_ONE_LEVEL");
    listings[0]=0;out.truncated=0;
    status=company_list(library_dir,library_name,1,&out);
    check(!status&&wcsstr(listings,library_file),"RECURSIVE_SEARCH_RETURNS_VERSIONED_PRT");
    dump_report(L"company-design-search.txt",listings);
    HANDLE checked=company_open_document(workspace_file,library_dir,verified);
    check(checked==INVALID_HANDLE_VALUE,"SAME_NAME_OUTSIDE_LIBRARY_REJECTED");
    if(checked!=INVALID_HANDLE_VALUE)CloseHandle(checked);
    checked=company_open_document(library_file,library_dir,verified);
    check(checked!=INVALID_HANDLE_VALUE,"NESTED_MODEL_PATH_AUTHORIZED");
    ProMdl before=NULL,after=NULL;
    ProMdlCurrentGet(&before);
    if(checked!=INVALID_HANDLE_VALUE){
        status=company_library_model(verified,model_report,_countof(model_report));
        dump_report(L"company-design-model.txt",model_report);
        check(!status&&wcsstr(model_report,L"50 × 30 × 6 mm")&&wcsstr(model_report,L"Ø8")&&
              wcsstr(model_report,L"CSYS:")&&wcsstr(model_report,L"原生特征顺序"),"PRT_DIMENSIONS_HOLE_CSYS_AND_FEATURES");
        check(!status&&company_source_pin(verified),"EXACT_VERSION_PINNED");
        ProMdl sourced=NULL;
        ProBoolean dirty=PRO_B_TRUE;
        if(!status&&!ProMdlnameInit(library_name,PRO_MDL_PART,&sourced)&&sourced){
            ProError stamp=ProMdlVerstampGet(sourced,&library_model_stamp);
            ProError modified=ProMdlModificationVerify(sourced,&dirty);
            fprintf(receipt,"READBACK_MODEL_STAMP=%d DIRTY_STATUS=%d DIRTY=%d\n",stamp,modified,dirty);
            fflush(receipt);
        }
        CloseHandle(checked);
    }
    ProMdlCurrentGet(&after);
    check(before==after,"LIBRARY_READ_PRESERVES_ACTIVE_MODEL");
    check(test_project_path(),"PROJECT_DISPATCH_CHANGES_TO_USER_SPECIFIED_E_DIRECTORY");
    const char *stage=NULL;
    status=model_create_empty(3,assembly_name,&stage);
    if(!status)status=ProMdlnameInit(assembly_name,PRO_MDL_ASSEMBLY,&assembly);
    check(!status&&assembly!=NULL,"EMPTY_PROJECT_ASSEMBLY_CREATED");
    if(!status&&!failures)status=ProMacroLoad(L"~ Command \x0060" L"BWTLCompanyDesignAssembly\x0060;");
    if(status||failures)finish();
    return 0;
}

static ProError assembly_protocol(const wchar_t *protocol,const wchar_t *matches){
    PartCommand parsed;
    PartSnapshot snapshot={0};
    wchar_t details[PART_TEXT_MAX]=L"";
    ProError error=part_snapshot_take_mode(assembly,&snapshot,0);
    if(!error)error=part_command_parse(protocol,&parsed)==1?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    if(!error)error=part_execute(&parsed,&snapshot,details,_countof(details));
    dump_report(L"company-design-assembly.txt",protocol);
    dump_report(L"company-design-assembly.txt",details);
    part_snapshot_clear(&snapshot);
    return error?error:matches&&!wcsstr(details,matches)?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static int test_assembly(uiCmdCmdId command,uiCmdValue *value,void *data){
    wchar_t protocol[512];
    check(storage_prepare_output(),"PROJECT_OUTPUT_ON_E_DRIVE");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_INSERT %ls %ls DEFAULT",assembly_name,library_name);
    ProError status=assembly_protocol(protocol,L"原生默认定位");
    check(!status,"INSERT_AND_NATIVE_DEFAULT_PLACEMENT");
    fprintf(receipt,"INSERT_CODE=%d\n",status);
    AxScan *scan=NULL;
    ProPath mounted;
    HANDLE reference=CreateFileW(library_file,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    int exact=reference!=INVALID_HANDLE_VALUE&&!ax_scan(assembly,1,&scan)&&scan->count==1&&
        scan->occurrences[0].model&&!ProMdlOriginGet(scan->occurrences[0].model,mounted)&&
        attachment_same_file(mounted,reference);
    check(exact,"INSERTED_SOURCE_IDENTIFIES_EXACT_LIBRARY_FILE_NOT_WORKDIR_COPY");
    if(exact){
        ProMdl source=scan->occurrences[0].model;
        ProBoolean dirty=PRO_B_TRUE;
        ProError modified=ProMdlModificationVerify(source,&dirty);
        ProError stamp=ProMdlVerstampGet(source,&placed_model_stamp);
        int same=!stamp&&library_model_stamp&&placed_model_stamp&&ProVerstampEqual(library_model_stamp,placed_model_stamp);
        fprintf(receipt,"FIRST_PLACEMENT_STAMP=%d DIRTY_STATUS=%d DIRTY=%d MATCHES_LIBRARY_READ_STAMP=%d\n",stamp,modified,dirty,same);
        fflush(receipt);
    }
    if(reference!=INVALID_HANDLE_VALUE)CloseHandle(reference);
    free(scan);

    ProError next=ProMacroLoad(L"~ Command \x0060" L"BWTLCompanyDesignFinish\x0060;");
    if(next){check(0,"SCHEDULE_NATIVE_FOLLOWUP");finish();}
    return 0;
}

static int test_followup(uiCmdCmdId command,uiCmdValue *value,void *data){
    wchar_t protocol[512];
    ProMdl model=NULL;
    ProBoolean dirty=PRO_B_TRUE;
    ProPath origin=L"";
    ProError lookup=ProMdlnameInit(library_name,PRO_MDL_PART,&model);
    ProError mark=lookup?lookup:ProMdlModificationVerify(model,&dirty);
    ProError identity=lookup?lookup:ProMdlOriginGet(model,origin);
    BY_HANDLE_FILE_INFORMATION current;
    HANDLE reference=company_source_open(library_file,&current);
    CompanySource *pin=company_source_find(library_name,PRO_MDL_PART);
    int disk_same=reference!=INVALID_HANDLE_VALUE&&pin&&company_source_matches(&pin->stamp,&current);
    int exact=reference!=INVALID_HANDLE_VALUE&&!identity&&attachment_same_file(origin,reference);
    if(reference!=INVALID_HANDLE_VALUE)CloseHandle(reference);
    ProWVerstamp loaded_stamp=NULL;
    ProError stamp_status=lookup?lookup:ProMdlVerstampGet(model,&loaded_stamp);
    int stamp_same=!stamp_status&&library_model_stamp&&loaded_stamp&&ProVerstampEqual(library_model_stamp,loaded_stamp);
    int placed_same=!stamp_status&&placed_model_stamp&&loaded_stamp&&ProVerstampEqual(placed_model_stamp,loaded_stamp);
    fprintf(receipt,"FOLLOWUP_MODEL_MATCHES_PLACED_STAMP=%d\n",placed_same);
    if(loaded_stamp)ProVerstampFree(&loaded_stamp);
    fprintf(receipt,"REPEAT_SOURCE_LOOKUP=%d MOD_STATUS=%d DIRTY=%d DISK_STAMP_UNCHANGED=%d EXACT_ORIGIN=%d MODEL_VERSION_UNCHANGED=%d\n",lookup,mark,dirty,disk_same,exact,stamp_same);
    fflush(receipt);
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_INSERT %ls %ls 10 0 0 FIX",assembly_name,library_name);
    check(!assembly_protocol(protocol,L"已放入"),"SECOND_NATIVE_FIXED_COMPONENT");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_INTERFERENCE %ls ALL",assembly_name);
    check(!assembly_protocol(protocol,L"正体积干涉 1 对"),"ACTUAL_INTERFERENCE_READBACK");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_Q_REGEN %ls",assembly_name);
    check(!assembly_protocol(protocol,NULL),"ASSEMBLY_REGEN_STATUS");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP SAVE %ls",assembly_name);
    check(!assembly_protocol(protocol,L"保存成功"),"NATIVE_PROJECT_MODEL_SAVE");
    ProPath native_file;
    check(!ax_native_source(project_dir,assembly_name,PRO_MDL_ASSEMBLY,native_file)&&dv_file_nonempty(native_file),"SAVED_NATIVE_ASSEMBLY_FILE_EXISTS");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP ASM_STEP %ls deliverables",assembly_name);
    check(!assembly_protocol(protocol,L"已导出装配 STEP"),"ASSEMBLY_STEP_EXPORT");
    ProPath folder,step;
    swprintf_s(folder,_countof(folder),L"%ls\\deliverables",project_dir);
    swprintf_s(step,_countof(step),L"%ls\\%ls.stp",folder,assembly_name);
    int exists=dv_file_nonempty(step);
    if(!exists){
        swprintf_s(step,_countof(step),L"%ls\\%ls.step",folder,assembly_name);
        exists=dv_file_nonempty(step);
    }
    check(exists,"NONEMPTY_STEP_IN_E_PROJECT_DIRECTORY");
    finish();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError error=company_design_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId cmd;
    if(!error)error=ProCmdActionAdd("BWTLCompanyDesignSetup",design_setup,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLCompanyDesignLibrary",test_library,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLCompanyDesignAssembly",test_assembly,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!error)error=ProCmdActionAdd("BWTLCompanyDesignFinish",test_followup,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLCompanyDesignSetup\x0060;");
}
