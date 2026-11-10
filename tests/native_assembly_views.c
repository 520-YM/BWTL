
#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl views_root,views_first,views_second;
static ProMdlName views_name,views_first_name,views_second_name;
static ProName views_interface;

static int views_run(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProError error=0;
    ProAsmcomp a={0},b={0};
    ProMdl copy=NULL;
    ProPath directory,restored;
    ProWVerstamp before=NULL,after=NULL;
    wchar_t line[2048];
#define NATIVE(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
#define CASE(reject,expect,format,...) do{swprintf_s(line,_countof(line),L"CREO_OP " format,views_name,__VA_ARGS__);command(views_root,line,reject,expect);}while(0)
    NATIVE(add_component(views_root,views_first,0,&a));
    NATIVE(add_component(views_root,views_first,150,&b));
    NATIVE(part_regenerate(views_root));
    CASE(0,L"已建立",L"ASM_V_REP %ls MODULES ID:%d ID:%d",a.id,b.id);
    CASE(1,NULL,L"ASM_V_REP %ls MODULES ID:%d",a.id);
    CASE(0,L"已建立并激活",L"ASM_V_ISOLATE %ls ONLY_ONE ID:%d",a.id);
    CASE(0,L"当前表示",L"ASM_V_ISOLATE %ls MASTER%ls",L"");
    CASE(0,L"当前表示",L"ASM_V_ISOLATE %ls APPLY ONLY_ONE%ls",L"");
    CASE(0,L"当前表示",L"ASM_V_ISOLATE %ls MASTER%ls",L"");
    CASE(0,L"已记录",L"ASM_V_VIEW %ls SERVICE SAVE%ls",L"");
    CASE(1,NULL,L"ASM_V_VIEW %ls SERVICE SAVE%ls",L"");
    CASE(0,L"已恢复",L"ASM_V_VIEW %ls SERVICE APPLY%ls",L"");
    CASE(0,L"已显示",L"ASM_V_SECTION %ls SLICE PLANE:ASM_FRONT 0%ls",L"");
    CASE(0,L"已显示",L"ASM_V_SECTION %ls SLICE APPLY%ls",L"");
    CASE(0,L"已显示",L"ASM_V_SECTION %ls SLICE_OFFSET PLANE:ASM_FRONT 5%ls",L"");
    CASE(0,L"已建立并显示",L"ASM_V_EXPLODE %ls EXP ID:%d 0 0 25 ID:%d 20 0 0",a.id,b.id);
    CASE(0,L"已恢复",L"ASM_V_EXPLODE %ls RESET%ls",L"");
    CASE(0,L"已显示",L"ASM_V_EXPLODE %ls EXP APPLY%ls",L"");
    CASE(0,L"已恢复",L"ASM_V_EXPLODE %ls RESET%ls",L"");
    CASE(0,L"2 步",L"ASM_V_SEQUENCE %ls STEPS ID:%d ID:%d",b.id,a.id);
    check(dv_file_nonempty(L"STEPS\\sequence.svg"),"SVG delivered");
    CASE(1,NULL,L"ASM_V_SEQUENCE %ls STEPS ID:%d",a.id);
    CASE(0,L"3 条",L"ASM_V_CONFIG %ls OPTIONS LARGE ID:%d %ls %ls CSYS:%ls LARGE ID:%d %ls %ls CSYS:%ls SINGLE ID:%d %ls %ls CSYS:%ls",
        a.id,views_first_name,views_second_name,views_interface,b.id,views_first_name,views_second_name,views_interface,
        a.id,views_first_name,views_second_name,views_interface);
    CASE(1,NULL,L"ASM_V_CONFIG %ls INVALID SINGLE ID:%d %ls %ls CSYS:MISSING",
        a.id,views_first_name,views_second_name);
    CASE(1,L"找不到配置",L"ASM_V_APPLY %ls OPTIONS MISSING%ls",L"");
    NATIVE(ProMdlVerstampGet(views_root,&before));
    NATIVE(ProDirectoryCurrentGet(directory));
    CASE(0,L"已输出订单装配",L"ASM_V_ORDER %ls OPTIONS SINGLE VIEW_ORDER ORDER_FILES%ls",L"");
    check(GetFileAttributesW(L"ORDER_FILES\\view_order.asm.1")==INVALID_FILE_ATTRIBUTES,"unconfigured intermediate revision omitted");
    NATIVE(ProDirectoryCurrentGet(restored));
    check(!_wcsicmp(directory,restored),"order preserves working directory");
    NATIVE(ProMdlVerstampGet(views_root,&after));
    check(ProVerstampEqual(before,after),"order preserves source stamp");
    ProMdl original=NULL;
    NATIVE(ProAsmcompMdlGet(&a,&original));
    check(original==views_first,"order preserves source component");
    ProPath order;
    swprintf_s(order,_countof(order),L"%ls\\ORDER_FILES\\view_order.asm",directory);
    error=ProMdlFiletypeLoad(order,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&copy);
    trace("load delivered assembly",error);
    if(!error){
        AxScan *scan=NULL;
        NATIVE(ax_scan(copy,1,&scan));
        int old=0,new_count=0;
        for(int i=0;i<scan->count;i++){
            old+=scan->occurrences[i].model==views_first;
            new_count+=scan->occurrences[i].model==views_second;
        }
        check(old==1&&new_count==1,"saved order holds selected configuration");
        free(scan);
        CASE(0,L"型号变更 1",L"ASM_V_COMPARE %ls VIEW_ORDER NONE 0.01%ls",L"");
    }else check(0,"order reload");
    CASE(0,L"2 个替换",L"ASM_V_APPLY %ls OPTIONS LARGE%ls",L"");
    AxScan *scan=NULL;
    NATIVE(ax_scan(views_root,1,&scan));
    int replaced=0;
    for(int i=0;i<scan->count;i++)replaced+=scan->occurrences[i].model==views_second;
    check(scan->count==2&&replaced==2,"apply replaced both occurrences");
    free(scan);
    CASE(1,NULL,L"ASM_V_APPLY %ls OPTIONS LARGE%ls",L"");
done:
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    trace("VIEWS_CASES",cases);
    trace("VIEWS_FAILURES",failures);
    trace("VIEWS_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CASE
#undef NATIVE
}

static int views_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    Fixture first,second;
    const char *stage="views assembly fixture";
    model_background_mode=0;
    ProError error=fixture(&views_first,views_first_name,&first);
    if(!error)error=fixture(&views_second,views_second_name,&second);
    if(!error)error=ProSolidFeatVisit((ProSolid)views_first,csys_name,NULL,views_interface);
    if(!error)error=ProMdlSave(views_first);
    if(!error)error=ProMdlSave(views_second);
    if(!error)error=model_create_empty(3,views_name,&stage);
    if(!error)error=ProMdlnameInit(views_name,PRO_MDL_ASSEMBLY,&views_root);
    if(!error)error=ProMacroLoad(L"~ Command \140BWTLViewsRun\140;");
    if(error){trace("VIEWS_DONE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLViewsPrepare",views_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLViewsRun",views_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLViewsPrepare\140;");
}
