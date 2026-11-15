#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"
#undef BWTL_REUSE_EXPANSION_PATTERN

static ProError standard_size_trial(FILE *log)
{
    ProPath profile,expected;
    if(!GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile)))return PRO_TK_BAD_CONTEXT;
    swprintf_s(expected,_countof(expected),L"%ls\\data",profile);
    if(_wcsicmp(runtime_data,expected))return PRO_TK_BAD_CONTEXT;
    ProMdl source=NULL;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&source,name,&geometry);
    wchar_t material_name[]=L"metal_steel_low_carbon";
    const wchar_t material_file[]=L"C:\\Program Files\\PTC\\Creo 10.0.0.0\\Common Files\\creo_standards\\material_database\\metal_steel_low_carbon.mtl";
    if(!error&&!CopyFileW(material_file,L"metal_steel_low_carbon.mtl",TRUE))error=PRO_TK_CANT_WRITE;
    if(!error)error=ProMaterialfileRead((ProSolid)source,material_name);
    ProMaterial source_material={0};
    source_material.part=(ProSolid)source;
    wcscpy_s(source_material.matl_name,_countof(source_material.matl_name),material_name);
    if(!error)error=ProMaterialCurrentSet(&source_material);
    if(!error)error=part_regenerate(source);
    ProPath delivered=L"",archived=L"",directory,path;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    ImitationOutcome outcome={0};
    if(!error)error=imitation_run_mode_impl(source,delivered,0,1,&outcome,&out);
    if(!error&&!outcome.edit_verified)error=PRO_TK_INCOMPLETE;
    if(!error&&!imitation_record(delivered,&imitation_last,source,&outcome,&out))error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_save_library(&imitation_last,&out);
    if(!error)error=imitation_standard_directory(directory);
    if(!error){
        const wchar_t *leaf=wcsrchr(delivered,L'\\');
        if(!leaf||swprintf_s(archived,_countof(archived),L"%ls%ls",directory,leaf)<0)error=PRO_TK_BAD_INPUTS;
    }
    if(!error&&imitation_standard_verify(delivered,path)!=PRO_TK_BAD_CONTEXT)error=PRO_TK_INCOMPLETE;
    if(!error){
        ProPath unverified,receipt;
        swprintf_s(unverified,_countof(unverified),L"%ls\\unchecked.prt.1",directory);
        swprintf_s(receipt,_countof(receipt),L"%ls.验收.txt",unverified);
        if(!CopyFileW(archived,unverified,TRUE))error=PRO_TK_CANT_WRITE;
        if(!error)error=imitation_library_write_receipt(receipt,L"BWTL 仿照验收\n源模型 测试\n尺寸试改 本次未执行\n");
        if(!error&&imitation_standard_verify(unverified,path)!=PRO_TK_INCOMPLETE)error=PRO_TK_INCOMPLETE;
    }
    fwprintf(log,L"ARCHIVE PATH CHECK=%d\n%ls\n",error,report);
    report[0]=0;
    out=(PartText){report,_countof(report),0};
    ProMdl standard=NULL;
    if(!error)error=imitation_open_standard(archived,&standard,&out);
    fwprintf(log,L"OPEN VERIFIED STANDARD=%d\n%ls\n",error,report);
    ProWVerstamp before=NULL,after=NULL;
    if(!error)error=ProMdlVerstampGet(standard,&before);
    BY_HANDLE_FILE_INFORMATION disk_before,disk_after;
    if(!error&&!imitation_file_info(archived,&disk_before))error=PRO_TK_CANT_OPEN;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(!error&&!snapshot)error=PRO_TK_OUT_OF_MEMORY;
    if(!error)error=part_snapshot_take_basic(standard,snapshot);
    PartCommand command;
    if(!error){
        wchar_t instruction[512];
        error=ProMdlMdlnameGet(standard,name);
        swprintf_s(instruction,_countof(instruction),L"CREO_OP IMITATE_SIZE %ls 90 60 20",name);
        if(!error&&part_command_parse(instruction,&command)!=1)error=PRO_TK_BAD_INPUTS;
    }
    report[0]=0;
    if(!error)error=imitation_resize_command(&command,snapshot,report,_countof(report));
    fwprintf(log,L"NEW SPECIFICATION=%d\n%ls\n",error,report);
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
    ProMdl current=NULL;
    double size[3]={0};
    if(!error)error=ProMdlCurrentGet(&current);
    if(!error&&current==standard)error=PRO_TK_INCOMPLETE;
    if(!error)error=imitation_extents(current,1,size);
    if(!error&&(fabs(size[0]-90)>0.002||fabs(size[1]-60)>0.002||fabs(size[2]-20)>0.002))
        error=PRO_TK_INCOMPLETE;
    ProMaterial result_part={0},result_body={0};
    if(!error)error=imitation_material_read(current,&result_part,&result_body);
    if(!error&&(_wcsicmp(result_part.matl_name,material_name)||_wcsicmp(result_body.matl_name,material_name)))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SPECIFICATION MATERIAL=%d\n",error);
    if(!error)error=ProMdlVerstampGet(standard,&after);
    if(!error&&!ProVerstampEqual(before,after))error=PRO_TK_INCOMPLETE;
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    if(!error&&(!imitation_file_info(archived,&disk_after)||
       CompareFileTime(&disk_before.ftLastWriteTime,&disk_after.ftLastWriteTime)||
       disk_before.nFileSizeHigh!=disk_after.nFileSizeHigh||disk_before.nFileSizeLow!=disk_after.nFileSizeLow))
        error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SIZE %.8g %.8g %.8g SOURCE PRESERVED=%d\n",size[0],size[1],size[2],error);
    return error;
}

static int standard_size_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-standard-size.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError error=standard_size_trial(log);
    fwprintf(log,L"PASS=%d ERROR=%d\n",!error,error);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=expansion_pattern_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayStandardSize",standard_size_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayStandardSize\x0060;");
}
