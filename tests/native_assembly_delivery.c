
#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl delivery_root,delivery_prior,delivery_sub,delivery_fast,delivery_wire,delivery_old;
static ProMdlName delivery_name,delivery_prior_name,delivery_sub_name,delivery_fast_name,delivery_wire_name,delivery_old_name;

static ProError delivery_param(ProModelitem *owner,const wchar_t *name,const wchar_t *value){
    ProParamvalue param={0};
    ProParameter parameter;
    param.type=PRO_PARAM_STRING;
    wcscpy_s(param.value.s_val,_countof(param.value.s_val),value);
    return ProParameterWithUnitsCreate(owner,(wchar_t *)name,&param,NULL,&parameter);
}

static ProError delivery_model_param(ProMdl model,const wchar_t *name,const wchar_t *value){
    ProModelitem item;
    ProError error=ProMdlToModelitem(model,&item);
    return error?error:delivery_param(&item,name,value);
}

static int delivery_contains(const wchar_t *path,const wchar_t *needle){
    FILE *file=_wfopen(path,L"rt, ccs=UTF-8");
    wchar_t row[8192];
    int result=0;
    if(!file)return 0;
    while(!result&&fgetws(row,_countof(row),file))result=wcsstr(row,needle)!=NULL;
    fclose(file);
    return result;
}

static int delivery_file(const wchar_t *path,const char *contents){
    FILE *file=_wfopen(path,L"wb");
    if(!file)return 0;
    int ok=fputs(contents,file)>=0;
    return fclose(file)==0&&ok;
}

static int delivery_run(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProError error=0;
    ProAsmcomp a={0},b={0},c={0},sub={0},prior={0};
    ProWVerstamp before=NULL,after=NULL;
    ProPath source_before,source_after,work,work_after,leaf;
    wchar_t line[2048],nested[64],path[PRO_PATH_SIZE];
#define NATIVE(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
#define CASE(reject,expect,format,...) do{swprintf_s(line,_countof(line),L"CREO_OP " format,delivery_name,__VA_ARGS__);command(delivery_root,line,reject,expect);}while(0)
    NATIVE(add_component(delivery_sub,delivery_fast,0,&a));
    NATIVE(add_component(delivery_sub,delivery_fast,30,&b));
    NATIVE(add_component(delivery_root,delivery_sub,0,&sub));
    NATIVE(add_component(delivery_root,delivery_wire,90,&c));
    NATIVE(add_component(delivery_prior,delivery_old,0,&prior));
    NATIVE(delivery_param(&c,L"BWTL_POSITION",L"PANEL_2"));
    NATIVE(delivery_param(&a,L"BWTL_TORQUE",L"2.5 N m"));
    NATIVE(delivery_param(&a,L"BWTL_CHECK",L"MARK_OK"));
    NATIVE(part_regenerate(delivery_sub));
    NATIVE(part_regenerate(delivery_prior));
    NATIVE(part_regenerate(delivery_root));
    NATIVE(ProMdlSave(delivery_sub));
    NATIVE(ProMdlSave(delivery_prior));
    NATIVE(ProMdlSave(delivery_root));
    NATIVE(ProMdlVerstampGet(delivery_root,&before));
    NATIVE(ProMdlOriginGet(delivery_root,source_before));
    NATIVE(ProDirectoryCurrentGet(work));
    swprintf_s(nested,_countof(nested),L"ID:%d/%d",sub.id,a.id);
    CASE(0,L"hierarchy.csv",L"ASM_B_TREE %ls HIER%ls",L"");
    check(delivery_contains(L"HIER\\hierarchy.csv",nested),"hierarchy includes nested path");
    CASE(1,NULL,L"ASM_B_TREE %ls HIER%ls",L"");
    CASE(0,L"procurement.csv",L"ASM_B_BUY %ls BUY%ls",L"");
    check(delivery_contains(L"BUY\\procurement.csv",L"标准件")&&delivery_contains(L"BUY\\procurement.csv",L"外购件"),"procurement class labels");
    CASE(0,L"bom-diff.csv",L"ASM_B_DIFF %ls DIFF %ls",delivery_prior_name);
    check(delivery_contains(L"DIFF\\bom-diff.csv",L"\"A\",\"B\",\"1\",\"2\",\"STEEL\",\"INOX\""),"BOM diff preserves quantities, revisions and materials");
    CASE(0,L"缺材料",L"ASM_B_CHECK %ls%ls",L"");
    CASE(0,L"bom-check.csv",L"ASM_B_CHECK %ls CHECK%ls",L"");
    check(delivery_contains(L"CHECK\\bom-check.csv",L"资料缺项"),"missing metadata reported");
    CASE(0,L"1 条记录",L"ASM_B_FAST %ls FAST%ls",L"");
    check(delivery_contains(L"FAST\\fasteners.csv",L"\"M4\",\"10 mm\",\"BASE\",\"2\""),"fastener count grouped");
    CASE(0,L"1 条记录",L"ASM_B_WIRE %ls WIRE%ls",L"");
    check(delivery_contains(L"WIRE\\wiring.csv",L"PANEL_2"),"wire uses occurrence position override");
    CASE(0,L"1 条记录",L"ASM_B_SPARE %ls SPARE%ls",L"");
    check(delivery_contains(L"SPARE\\spares.csv",L"可独立更换"),"spare recognized");
    CASE(0,L"2 条记录",L"ASM_B_ROUTE %ls ROUTE %ls ID:%d",nested,c.id);
    check(delivery_contains(L"ROUTE\\process.csv",L"2.5 N m")&&delivery_contains(L"ROUTE\\process.csv",L"MARK_OK"),"process includes occurrence torque and checkpoint");
    CASE(1,NULL,L"ASM_B_ROUTE %ls INVALID ID:99999%ls",L"");
    check(GetFileAttributesW(L"INVALID")==INVALID_FILE_ATTRIBUTES,"invalid route has no partial output");
    CASE(0,L"4 个原生文件，0 项未核实",L"ASM_B_NATIVE %ls NATIVE_FILES%ls",L"");
    check(delivery_contains(L"NATIVE_FILES\\native-files.csv",delivery_sub_name)&&
          delivery_contains(L"NATIVE_FILES\\native-files.csv",L"已收集"),"dependency manifest lists subassembly");
    check(delivery_file(L"drawing.pdf","%PDF-1.4\n%%EOF\n")&&delivery_file(L"check.txt","check completed\n"),"input artifacts");
    CASE(0,L"装配交付包已生成",L"ASM_B_PACK %ls DELIVER drawing.pdf check.txt%ls",L"");
    check(dv_file_nonempty(L"DELIVER\\hierarchy.csv")&&dv_file_nonempty(L"DELIVER\\assembly-drawing.pdf")&&
          dv_file_nonempty(L"DELIVER\\inspection-report.txt"),"delivery contains native reports and attachments");
    CASE(1,NULL,L"ASM_B_PACK %ls MISSING drawing.pdf absent.txt%ls",L"");
    check(GetFileAttributesW(L"MISSING")==INVALID_FILE_ATTRIBUTES,"missing attachment has no partial output");
    check(delivery_file(L"fake.pdf","not a PDF"),"invalid drawing fixture");
    CASE(1,NULL,L"ASM_B_PACK %ls BAD_PDF fake.pdf check.txt%ls",L"");
    check(GetFileAttributesW(L"BAD_PDF")==INVALID_FILE_ATTRIBUTES,"invalid drawing does not publish delivery");
    NATIVE(ProMdlVerstampGet(delivery_root,&after));
    NATIVE(ProMdlOriginGet(delivery_root,source_after));
    NATIVE(ProDirectoryCurrentGet(work_after));
    check(ProVerstampEqual(before,after)&&!_wcsicmp(source_before,source_after)&&!_wcsicmp(work,work_after),
          "source version, path and work directory remain intact");
    swprintf_s(leaf,_countof(leaf),L"%ls.asm",delivery_name);
    swprintf_s(path,_countof(path),L"%ls\\DELIVER\\%ls",work,leaf);
    if(dv_file_nonempty(L"DELIVER\\native-files.csv")){
        wchar_t expected[65],actual[65];
        ULONGLONG size,copy_size;
        AxPackModel root={delivery_root,PRO_MDL_ASSEMBLY,L""};
        wcscpy_s(root.name,_countof(root.name),delivery_name);
        ProPath saved=L"";
        error=ax_pack_saved_path(&root,saved);
        if(!error){
            const wchar_t *name=wcsrchr(saved,L'\\');
            swprintf_s(path,_countof(path),L"%ls\\DELIVER\\%ls",work,name+1);
            check(dv_sha256(saved,expected,&size)&&dv_sha256(path,actual,&copy_size)&&size==copy_size&&!wcscmp(expected,actual),
                "delivered root matches source bytes");
        }else check(0,"read source file for checksum");
        error=0;
    }
    NATIVE(delivery_model_param(delivery_fast,L"UNSAVED_FIXTURE",L"YES"));
    CASE(0,L"1 项未核实",L"ASM_B_NATIVE %ls PARTIAL%ls",L"");
    CASE(1,L"1 项依赖未核实",L"ASM_B_PACK %ls INCOMPLETE drawing.pdf check.txt%ls",L"");
    check(GetFileAttributesW(L"INCOMPLETE")==INVALID_FILE_ATTRIBUTES,"incomplete delivery not published");
done:
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    trace("DELIVERY_CASES",cases);
    trace("DELIVERY_FAILURES",failures);
    trace("DELIVERY_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CASE
#undef NATIVE
}

static int delivery_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    Fixture f;
    const char *stage="assembly delivery fixture";
    model_background_mode=0;
    ProError error=fixture(&delivery_fast,delivery_fast_name,&f);
    if(!error)error=fixture(&delivery_wire,delivery_wire_name,&f);
    if(!error)error=fixture(&delivery_old,delivery_old_name,&f);
    static const wchar_t *const attrs[][3]={
        {L"PART_CODE",L"F-001",L"F-001"},{L"DRAWING_NO",L"DW-01",L"DW-01"},
        {L"REVISION",L"B",L"A"},{L"MATERIAL",L"INOX",L"STEEL"},{L"SPEC",L"M4",L"M4"},
        {L"BWTL_SOURCE",L"STANDARD",L"STANDARD"},{L"BWTL_KIND",L"FASTENER",L"FASTENER"},
        {L"BWTL_LENGTH",L"10 mm",L"10 mm"},{L"BWTL_AREA",L"BASE",L"BASE"}
    };
    for(int i=0;!error&&i<_countof(attrs);i++){
        error=delivery_model_param(delivery_fast,attrs[i][0],attrs[i][1]);
        if(!error)error=delivery_model_param(delivery_old,attrs[i][0],attrs[i][2]);
    }
    if(!error)error=delivery_model_param(delivery_wire,L"BWTL_KIND",L"HARNESS");
    if(!error)error=delivery_model_param(delivery_wire,L"BWTL_SOURCE",L"BUY");
    if(!error)error=delivery_model_param(delivery_wire,L"BWTL_SPARE",L"YES");
    if(!error)error=delivery_model_param(delivery_wire,L"BWTL_AREA",L"DOOR");
    if(!error)error=ProMdlSave(delivery_fast);
    if(!error)error=ProMdlSave(delivery_wire);
    if(!error)error=ProMdlSave(delivery_old);
    if(!error)error=model_create_empty(3,delivery_sub_name,&stage);
    if(!error)error=ProMdlnameInit(delivery_sub_name,PRO_MDL_ASSEMBLY,&delivery_sub);
    if(!error)error=model_create_empty(3,delivery_prior_name,&stage);
    if(!error)error=ProMdlnameInit(delivery_prior_name,PRO_MDL_ASSEMBLY,&delivery_prior);
    if(!error)error=model_create_empty(3,delivery_name,&stage);
    if(!error)error=ProMdlnameInit(delivery_name,PRO_MDL_ASSEMBLY,&delivery_root);
    if(!error)error=ProMacroLoad(L"~ Command \140BWTLDeliveryRun\140;");
    if(error){trace("DELIVERY_DONE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLDeliveryPrepare",delivery_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLDeliveryRun",delivery_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLDeliveryPrepare\140;");
}
