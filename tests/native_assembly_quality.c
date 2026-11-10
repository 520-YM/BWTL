
#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl quality_root;
static ProMdlName quality_name,quality_stock_name;
static ProMdl quality_stock;
static Fixture quality_surfaces;

typedef struct {ProMdl model;int id;} QualityTestBore;

static ProError quality_bore_visit(ProSurface surface,ProError status,ProAppData context){
    if(status)return status;
    QualityTestBore *search=context;
    ProGeomitem item;
    if(ProSurfaceToGeomitem((ProSolid)search->model,surface,&item))return 0;
    wchar_t token[32];
    FdRef reference={0};
    QualityBore bore;
    swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
    if(!qa_bore(search->model,token,&reference,&bore))search->id=item.id;
    fd_ref_free(&reference);
    return 0;
}

static ProError quality_place(double x,double y,double z,ProAsmcomp *part){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}};
    return ProAsmcompAssemble((ProSolid)quality_root,quality_stock,pose,part);
}

static int quality_run(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProError error=0;
    ProAsmcomp a={0},b={0},c={0},d={0};
    ProSolidBody body;
    QualityTestBore bore={.model=quality_stock};
    ProWVerstamp original=NULL,current=NULL;
    wchar_t line[2048];
#define NATIVE(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
#define CASE(reject,expect,format,...) do{swprintf_s(line,_countof(line),L"CREO_OP " format,quality_name,__VA_ARGS__);command(quality_root,line,reject,expect);}while(0)
    NATIVE(quality_place(0,0,0,&a));
    NATIVE(quality_place(81,0,0,&b));
    NATIVE(quality_place(0,58,0,&c));
    NATIVE(quality_place(160,0,2,&d));
    NATIVE(part_regenerate(quality_root));
    NATIVE(qa_single_body(quality_stock,&body));
    NATIVE(ProSolidBodySurfaceVisit(&body,quality_bore_visit,&bore));
    if(!bore.id){error=-1;goto done;}
    NATIVE(ProMdlVerstampGet(quality_root,&original));
    CASE(1,L"暂无与当前装配",L"ASM_Q_REPORT %ls%ls",L"");
    CASE(0,L"扫描 4 个实例",L"ASM_Q_REGEN %ls%ls",L"");
    CASE(0,L"最小间隙 1 mm",L"ASM_Q_GAPS %ls SCREEN ID:%d ID:%d 0.9 PCB ID:%d ID:%d 1.1",a.id,b.id,a.id,b.id);
    CASE(0,L"未核实 1 项",L"ASM_Q_GAPS %ls GLASS ID:%d ID:99999 0.5",a.id);
    CASE(0,L"登记允许 1 对",L"ASM_Q_INTERF %ls ID:%d ID:%d 3300",c.id,a.id);
    CASE(0,L"异常干涉",L"ASM_Q_INTERF %ls ID:%d ID:%d 10",c.id,a.id);
    CASE(1,L"未核实",L"ASM_Q_INTERF %ls ID:%d ID:99999 2",a.id);
    CASE(0,L"偏心 81 mm",L"ASM_Q_HOLES %ls ID:%d SURFACE:%d ID:%d SURFACE:%d 1 1",a.id,bore.id,b.id,bore.id);
    CASE(0,L"最大段差 2 mm",L"ASM_Q_FLUSH %ls ID:%d SURFACE:%d ID:%d SURFACE:%d 0.5 1",a.id,quality_surfaces.top,d.id,quality_surfaces.top);
    CASE(0,L"不符合 1 项",L"ASM_Q_COPLANE %ls 0.5 1 ID:%d SURFACE:%d ID:%d SURFACE:%d ID:%d SURFACE:%d",
        a.id,quality_surfaces.top,b.id,quality_surfaces.top,d.id,quality_surfaces.top);
    CASE(0,L"[0.7, 4.4]",L"ASM_Q_STACK %ls 1 4 ADD GAP 30 1 2 SUB INSERT 28 0.4 0.3%ls",L"");
    CASE(0,L"不符合 0 项",L"ASM_Q_SUPPORT %ls CSYS:ASM_CSYS 5 -10 -10 250 -10 250 130 -10 130%ls",L"");
    CASE(0,L"不符合 1 项",L"ASM_Q_SUPPORT %ls CSYS:ASM_CSYS 0 0 0 10 0 10 10 0 10%ls",L"");
    CASE(0,L"+X 20",L"ASM_Q_SPACE %ls CSYS:ASM_CSYS 0 0 0 220 120 25 ID:%d",d.id);
    CASE(0,L"装配质量检查报告",L"ASM_Q_REPORT %ls%ls",L"");
    CASE(0,L"已输出报告",L"ASM_Q_REPORT %ls QUALITY_OUTPUT%ls",L"");
    check(dv_file_nonempty(L"QUALITY_OUTPUT\\report.txt"),"report actually written to working directory");
    CASE(1,L"已存在",L"ASM_Q_REPORT %ls QUALITY_OUTPUT%ls",L"");
    NATIVE(ProMdlVerstampGet(quality_root,&current));
    check(ProVerstampEqual(original,current),"read-only checks preserve top assembly version");
    ProVerstampFree(&current);
    ProMatrix pose;
    NATIVE(ax_actual_pose(quality_root,&d,pose));
    pose[3][0]+=10;
    NATIVE(ax_set_position(quality_root,&d,pose));
    ProMatrix changed;
    NATIVE(ax_actual_pose(quality_root,&d,changed));
    check(fabs(changed[3][0]-170)<1e-6,"fixture occurrence physically moved");
    CASE(1,L"暂无与当前装配",L"ASM_Q_REPORT %ls%ls",L"");
    CASE(0,L"不符合 0 项",L"ASM_Q_STACK %ls 0 5 ADD GAP 30 1 2 SUB INSERT 28 0.4 0.3%ls",L"");
    CASE(0,L"本报告无记录",L"ASM_Q_REPORT %ls%ls",L"");
    ProDimension dim;
    NATIVE(ProModelitemInit(quality_stock,0,PRO_DIMENSION,(ProModelitem *)&dim));
    double old;
    NATIVE(ProDimensionValueGet(&dim,&old));
    NATIVE(ProDimensionValueSet(&dim,old+1));
    CASE(1,L"暂无与当前装配",L"ASM_Q_REPORT %ls%ls",L"");
    CASE(0,L"待重生成",L"ASM_Q_REGEN %ls%ls",L"");
done:
    if(original)ProVerstampFree(&original);
    if(current)ProVerstampFree(&current);
    ax_q_clear(&ax_q_report_cache);
    trace("QUALITY_CASES",cases);
    trace("QUALITY_FAILURES",failures);
    trace("QUALITY_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CASE
#undef NATIVE
}

static int quality_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    model_background_mode=0;
    wchar_t line[1024];
    const char *stage="quality assembly fixture";
    ProError error=fixture(&quality_stock,quality_stock_name,&quality_surfaces);
    if(!error){
        swprintf_s(line,_countof(line),L"CREO_OP HOLE %ls QUALITY_BORE SURFACE:%d SURFACE:%d 10 SURFACE:%d 10 4 THROUGH 0",
            quality_stock_name,quality_surfaces.top,quality_surfaces.x,quality_surfaces.y);
        error=run(quality_stock,line,0);
    }
    if(!error&&!CopyFileW(L"C:/Program Files/PTC/Creo 10.0.0.0/Common Files/creo_standards/material_database/plastic_pvc.mtl",
        L"plastic_pvc.mtl",FALSE))error=PRO_TK_CANT_ACCESS;
    if(!error)error=material_assign(quality_stock);
    if(!error)error=model_create_empty(3,quality_name,&stage);
    if(!error)error=ProMdlnameInit(quality_name,PRO_MDL_ASSEMBLY,&quality_root);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLQualityRun`;");
    if(error){trace("QUALITY_DONE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLQualityPrepare",quality_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLQualityRun",quality_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLQualityPrepare`;");
}
