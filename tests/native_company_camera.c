#define user_initialize camera_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static const wchar_t camera_case_number[]=L"M26A0100000V00";
static const wchar_t camera_case_rename[]=L"M26A0200000V00";

static const wchar_t camera_case_saved_rename[]=L"M26A0300000V00";
static wchar_t camera_fixture_source[MAX_PATH];

static ProError camera_verify_product_files(const wchar_t *name,const wchar_t *extension){
    ProPath work,pattern;
    ProError error=ProDirectoryCurrentGet(work);
    if(error)return error;
    if(swprintf_s(pattern,PRO_PATH_SIZE,L"%ls\\%ls.%ls.*",work,name,extension)<0)return PRO_TK_BAD_INPUTS;
    WIN32_FIND_DATAW entry;
    HANDLE find=FindFirstFileW(pattern,&entry);
    if(find==INVALID_HANDLE_VALUE)return PRO_TK_E_NOT_FOUND;
    int found=0;
    do{
        if(!(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
           (entry.nFileSizeHigh||entry.nFileSizeLow)){found=1;break;}
    }while(FindNextFileW(find,&entry));
    FindClose(find);
    return found?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static ProError camera_test_library(ProPath local){
    ProPath root,standards,library;
    ProError error=imitation_library_paths(root,standards,library);
    if(error)return error;
    if(swprintf_s(local,PRO_PATH_SIZE,L"%ls\\环境摄像头",library)<0)return PRO_TK_BAD_INPUTS;
    if(!imitation_library_folder(root)||!imitation_library_folder(standards)||
       !imitation_library_folder(library)||!imitation_library_folder(local))return PRO_TK_CANT_WRITE;
    if(!GetEnvironmentVariableW(L"BWTL_TEST_CAMERA_MODELS",camera_fixture_source,_countof(camera_fixture_source)))
        return PRO_TK_BAD_INPUTS;
    if((error=camera_reference_settings(camera_fixture_source)))return error;
    wchar_t config[PRO_PATH_SIZE],digest[65];
    if(swprintf_s(config,_countof(config),L"%ls\\camera-models.ini",camera_fixture_source)<0)return PRO_TK_BAD_INPUTS;
    if((error=ax_pack_copy(config,L"camera-models.ini",local,digest)))return error;
    for(size_t i=0;i<_countof(camera_references);i++){
        ProPath from;
        wchar_t digest[65];
        if(swprintf_s(from,PRO_PATH_SIZE,L"%ls\\%ls",camera_fixture_source,camera_references[i].leaf)<0)
            return PRO_TK_BAD_INPUTS;
        error=ax_pack_copy(from,camera_references[i].leaf,local,digest);
        if(error)return error;
    }
    return camera_reference_folder(local);
}
static ProError camera_test_mount_refs(ProMdl assembly,ProAsmcomp *component,ProMdl bracket,int wanted){
    ProAsmcomppath root;
    ProAsmcompconstraint *rows=NULL;
    int count=0;
    ProError error=ax_root_path(assembly,&root);
    if(!error)error=ax_read_constraints(component,&root,&rows,&count);
    if(!error&&count!=wanted)error=PRO_TK_INCOMPLETE;
    for(int i=0;!error&&i<count;i++){
        ProSelection base=NULL;
        ProDatumside side;
        ProModelitem item;
        error=ProAsmcompconstraintAsmreferenceGet(rows[i],&base,&side);
        if(!error)error=ProSelectionModelitemGet(base,&item);
        if(!error&&item.owner!=bracket)error=PRO_TK_BAD_CONTEXT;
    }
    if(rows)ProAsmcompconstraintArrayFree(rows);
    return error;
}
static ProError camera_test_name(ProMdl model,const wchar_t *wanted){
    ProMdlName actual;
    ProError error=ProMdlMdlnameGet(model,actual);
    return error?error:_wcsicmp(actual,wanted)?PRO_TK_INCOMPLETE:PRO_TK_NO_ERROR;
}

static ProError camera_test_components(ProMdl assembly,ProMdl *bracket,PartText *out){
    AxScan *scan=NULL;
    ProError error=ax_scan(assembly,1,&scan);
    int brackets=0,electronics=0,standoffs=0,nuts=0;
    if(!error)for(int i=0;i<scan->count&&!error;i++){
        const AxOccurrence *row=&scan->occurrences[i];
        ProAsmcomp component;
        ProMdl child;
        ProMdlName name;
        if(row->state!=PRO_FEAT_ACTIVE||row->depth!=1){
            error=PRO_TK_INCOMPLETE;
            break;
        }
        error=ax_component_feature(assembly,row->ids[0],&component);
        if(!error)error=ProAsmcompMdlGet(&component,&child);
        if(!error)error=ProMdlMdlnameGet(child,name);
        if(error)break;
        if(!_wcsicmp(name,camera_case_number)){
            ProMdlsubtype subtype;
            error=ProMdlSubtypeGet(child,&subtype);
            if(!error&&subtype!=PROMDLSTYPE_PART_SHEETMETAL)error=PRO_TK_INVALID_TYPE;
            if(!error)error=ax_default_verify(&component);
            if(!error)*bracket=child;
            brackets++;
        }else if(!_wcsicmp(name,camera_references[1].name)){
            error=camera_test_mount_refs(assembly,&component,*bracket,3);
            electronics++;
        }else if(!_wcsicmp(name,camera_references[2].name)){
            error=camera_test_mount_refs(assembly,&component,*bracket,2);
            standoffs++;
        }else if(!_wcsicmp(name,camera_references[3].name)){
            error=camera_test_mount_refs(assembly,&component,*bracket,2);
            nuts++;
        }else error=PRO_TK_E_NOT_FOUND;
    }
    free(scan);
    if(!error&&(brackets!=1||electronics!=1||standoffs!=2||nuts!=2))error=PRO_TK_INCOMPLETE;
    part_text_add(out,L"CAMERA_COMPONENTS=%d,%d,%d,%d\n",brackets,electronics,standoffs,nuts);
    return error;
}

static ProError camera_open_saved(const wchar_t *directory,PartText *out){
    if(!storage_directory_on_drive(directory,L'E'))return PRO_TK_BAD_CONTEXT;
    ProError error=ProDirectoryChange((wchar_t *)directory);
    const wchar_t *names[]={camera_case_rename,camera_case_saved_rename};
    for(size_t i=0;!error&&i<_countof(names);i++){
        ProMdl model=NULL,bracket=NULL;
        int loaded=0;
        error=ax_load_subassembly(names[i],&model,&loaded);
        if(!error&&!loaded)error=PRO_TK_E_IN_USE;
        if(!error)error=camera_test_name(model,names[i]);
        if(!error)error=part_regenerate(model);
        if(!error)error=part_health(model);
        if(!error)error=camera_test_components(model,&bracket,out);
        part_text_add(out,L"已保存装配 %ls 重开并核对结果：Creo %d。\n",names[i],error);
    }
    return error;
}
static ProError camera_test_actual_chat(PartText *out){
    ProPath local;
    ProError error=camera_test_library(local);
    if(error)return error;
    if(!camera_save_requested(L"请保存")||camera_save_requested(L"先别保存")||
       camera_save_requested(L"暂不保存")||camera_save_requested(L"不要自动保存"))
        return PRO_TK_BAD_INPUTS;
    const wchar_t *request=L"请按公司标准画一个环境摄像头，图号 M26A0100000V00，保存到当前工作目录。";
    if(!camera_standard_chat(request))return PRO_TK_BAD_INPUTS;
    ProMdl model=NULL,bracket=NULL;
    ProMdlName name;
    wcscpy_s(name,_countof(name),camera_case_number);
    error=ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&model);
    if(!error)error=camera_test_name(model,camera_case_number);
    if(!error)error=camera_test_components(model,&bracket,out);
    if(!error)error=camera_verify_product_files(camera_case_number,L"prt");
    if(!error)error=camera_verify_product_files(camera_case_number,L"asm");
    return error;
}
static int camera_native_fixture(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *receipt=_wfopen(L"camera-standard-native.log",L"wt, ccs=UTF-8");
    if(!receipt){ProEngineerEnd();return 0;}
    static wchar_t report[64000];
    PartText out={report,_countof(report),0};
    if(GetEnvironmentVariableW(L"BWTL_CAMERA_CHAT_PROBE",NULL,0)){
        ProError actual=camera_test_actual_chat(&out);
        part_text_add(&out,L"CAMERA_ACTUAL_CHAT_PASS=%d\n",!actual);
        if(actual)part_text_add(&out,L"真实聊天入口未通过：Creo %d。\n",actual);
        fwprintf(receipt,L"%ls",report);
        fclose(receipt);
        ProEngineerEnd();
        return 0;
    }
    ProPath reopen;
    const DWORD reopen_length=GetEnvironmentVariableW(L"BWTL_CAMERA_REOPEN_DIR",reopen,_countof(reopen));
    if(reopen_length){
        ProError opened=reopen_length>=_countof(reopen)?PRO_TK_BAD_INPUTS:camera_open_saved(reopen,&out);
        part_text_add(&out,L"CAMERA_REOPEN_PASS=%d\n",!opened);
        fwprintf(receipt,L"%ls",report);
        fclose(receipt);
        ProEngineerEnd();
        return 0;
    }
    ProMdl asm_model=NULL,sheet=NULL;
    ProPath local,work;
    const wchar_t *stage=L"聊天意图识别";
    ProMdlName read;
    const int intent=camera_standard_requested(L"帮我画一个公司标准环境摄像头，图号 M26A0100000V00")&&
        !camera_standard_requested(L"不要新建环境摄像头")&&
        !camera_standard_requested(L"环境摄像头怎么画？")&&
        !camera_standard_requested(L"画一个环境摄像头支架")&&
        camera_drawing_number(L"帮我画一个公司标准环境摄像头，图号 M26A0100000V00",read)==1&&
        !_wcsicmp(read,camera_case_number);
    ProError error=intent?PRO_TK_NO_ERROR:PRO_TK_BAD_INPUTS;
    part_text_add(&out,L"CAMERA_INTENT=%d\n",intent);
    if(!error){
        stage=L"从当前实例的公司标准库加载验收样件";
        error=camera_test_library(local);
        if(!error)part_text_add(&out,L"CAMERA_LIBRARY=1\n");
        if(!error)error=camera_standard_build(local,camera_case_number,0,0,&asm_model,&out);
        if(!error)error=camera_test_name(asm_model,camera_case_number);
    }
    if(!error){
        stage=L"未保存的实际摄像头总装改名";
        error=assembly_rename_apply(asm_model,camera_case_rename,&out);
        if(!error)error=camera_test_name(asm_model,camera_case_rename);
        if(!error)error=camera_test_components(asm_model,&sheet,&out);
        if(!error)part_text_add(&out,L"CAMERA_RENAME=1\n");
    }
    if(!error){
        stage=L"保存改过名的总装和钣金件";
        error=ProDirectoryCurrentGet(work);
        if(!error)error=camera_prepare_dependency_files(local,work,&out);
        if(!error)error=ProMdlSave(sheet);
        if(!error)error=ProMdlSave(asm_model);
        if(!error)error=camera_verify_product_files(camera_case_number,L"prt");
        if(!error)error=camera_verify_product_files(camera_case_rename,L"asm");
        if(!error)part_text_add(&out,L"CAMERA_SAVED=1\n");
    }
    if(!error){
        stage=L"已保存的实际摄像头总装改名";
        error=assembly_rename_apply(asm_model,camera_case_saved_rename,&out);
        if(!error)error=camera_test_name(asm_model,camera_case_saved_rename);
        if(!error)error=camera_test_components(asm_model,&sheet,&out);
        if(!error)error=camera_verify_product_files(camera_case_rename,L"asm");
        if(!error)error=camera_verify_product_files(camera_case_saved_rename,L"asm");
        if(!error)part_text_add(&out,L"CAMERA_SAVED_RENAME=1\n");
    }
    if(error)part_text_add(&out,L"验收未完成阶段 %ls，Creo %d。\n",stage,error);
    part_text_add(&out,L"CAMERA_NATIVE_PASS=%d\n",!error);
    fwprintf(receipt,L"%ls",report);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError status=camera_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId action;
    if(!status)status=ProCmdActionAdd("BWTLCompanyCameraAcceptance",camera_native_fixture,uiProe2ndImmediate,NULL,
        PRO_B_TRUE,PRO_B_TRUE,&action);
    return status?status:ProMacroLoad(L"~ Command \x0060" L"BWTLCompanyCameraAcceptance\x0060;");
}