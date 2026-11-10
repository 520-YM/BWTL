#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProMdl root;
static ProMdlName asm_name;

typedef struct {ProMdl model;int id;} TestBore;
static ProError test_bore(ProSurface surface,ProError status,ProAppData context){
    if(status)return status;
    TestBore *data=context;
    ProGeomitem item;
    wchar_t token[32];
    FdRef ref={0};
    QualityBore bore;
    if(ProSurfaceToGeomitem((ProSolid)data->model,surface,&item))return 0;
    swprintf_s(token,_countof(token),L"SURFACE:%d",item.id);
    if(!qa_bore(data->model,token,&ref,&bore))data->id=item.id;
    fd_ref_free(&ref);
    return 0;
}

static ProError fixture_box(ProMdlName name,double width,double height,double depth,ProMdl *model){
    const char *stage="fieldwork stock";
    ModelPlan stock={0,width,height,depth,0,1};
    ProError error=quick_seed(name,model);
    return error?error:model_build_internal(&stock,name,&stage,*model,NULL,NULL);
}

static ProError fixture_component(ProMdl model,double x,double y,double z,ProAsmcomp *comp){
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}};
    return ProAsmcompAssemble((ProSolid)root,model,pose,comp);
}

static double fixture_volume(ProAsmcomp *component){
    ProMdl model=NULL;
    return ProAsmcompMdlGet(component,&model)?-1:volume(model);
}

static int run_fieldwork(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProMdl part=NULL,source=NULL,module=NULL;
    ProAsmcomp a={0},b={0},third={0},lower={0},window={0};
    ProWVerstamp stamp=NULL,after=NULL;
    ProMassProperty mass={0},cut_mass={0};
    ProError error;
    wchar_t line[1024],report[PART_TEXT_MAX]=L"";
    PartCommand sketch;
#define RUN(call) do{error=(call);trace(#call,error);if(error)goto done;}while(0)
#define CMD(expected,fmt,...) do{swprintf_s(line,_countof(line),L"CREO_OP " fmt,asm_name,__VA_ARGS__);RUN(command(root,line,0,expected));if(failures){error=-1;goto done;}}while(0)
#define DELTA(component,expected) do{double actual=fixture_volume(component);check(fabs(actual-(expected))<0.005,"actual occurrence volume");if(failures){error=-1;goto done;}}while(0)
    const double pi=3.141592653589793;
    double a_volume=51200;
    RUN(fixture_box(L"FIELD_STOCK",80,80,8,&part));
    RUN(fixture_component(part,0,0,0,&a));
    RUN(fixture_component(part,0,0,0,&b));
    RUN(fixture_component(part,100,0,0,&third));
    RUN(fixture_component(part,0,0,16,&lower));
    RUN(ProMdlVerstampGet(part,&stamp));
    RUN(qa_geometry_mass(root,&mass));
    trace("assembly volume before", (int)mass.volume);
    if(part_command_parse(L"CREO_OP SKETCH_CREATE A ROOT_CUT_PROFILE PLANE:ASM_FRONT PLANE:ASM_RIGHT CIRCLE 0 0 2",&sketch)!=1){error=-1;goto done;}
    ProVector point={8,8,0};
    RUN(fd_sketch_create_at(&sketch,root,point,report,_countof(report)));
    CMD(L"已核对 1 个指定实例",L"ASM_FIELD_CUT %ls CUSTOM ROOT_CUT_PROFILE POS 0 ID:%d",a.id);
    a_volume-=32*pi; DELTA(&a,a_volume); DELTA(&b,51200);
    RUN(qa_geometry_mass(root,&cut_mass));
    trace("assembly volume after first cut", (int)cut_mass.volume);
    CMD(L"另有 3 个同模型实例",L"ASM_FIELD_AUDIT %ls CUSTOM%ls",L"");
    CMD(L"核对了 2 个加工特征",L"ASM_FIELD_HOLE %ls THRU PLANE:ASM_FRONT PLANE:ASM_RIGHT POS 18 16 0 4 ID:%d ID:%d",a.id,lower.id);
    a_volume-=32*pi; DELTA(&a,a_volume); DELTA(&lower,51200-32*pi);
    CMD(L"已核对 1 个指定实例",L"ASM_FIELD_GRID %ls GRID PLANE:ASM_FRONT PLANE:ASM_RIGHT POS 34 34 0 3 2 2 10 10 ID:%d",a.id);
    a_volume-=72*pi; DELTA(&a,a_volume);
    Fixture source_faces;
    ProMdlName source_name;
    RUN(fixture(&source,source_name,&source_faces));
    swprintf_s(line,_countof(line),L"CREO_OP HOLE %ls GUIDE_BORE SURFACE:%d SURFACE:%d 10 SURFACE:%d 10 4 THROUGH 0",
        source_name,source_faces.top,source_faces.x,source_faces.y);
    RUN(run(source,line,0));
    int root_window;
    RUN(ProMdlWindowGet(root,&root_window));
    RUN(ProWindowCurrentSet(root_window));
    ProSolidBody body;
    TestBore bore={.model=source};
    RUN(qa_single_body(source,&body));
    RUN(ProSolidBodySurfaceVisit(&body,test_bore,&bore));
    if(!bore.id){error=-1;goto done;}
    ProAsmcomp guide;
    RUN(fixture_component(source,10,50,-25,&guide));
    CMD(L"已核对 1 个指定实例",L"ASM_FIELD_TRANSFER %ls TRANSFER PLANE:ASM_FRONT PLANE:ASM_RIGHT POS ID:%d SURFACE:%d 0 ID:%d",guide.id,bore.id,a.id);
    a_volume-=32*pi; DELTA(&a,a_volume);
    RUN(ax_layout_plane(root,L"LAYER_TWO",L"PLANE:ASM_FRONT",16));
    CMD(L"LAYER_3",L"ASM_FIELD_LAYERS %ls LAYER PLANE:ASM_RIGHT POS 62 20 0 ID:%d PLANE:ASM_FRONT CBORE 4 7 3 ID:%d PLANE:LAYER_TWO CORE 3 0 0",a.id,lower.id);
    a_volume-=(32+24.75)*pi; DELTA(&a,a_volume); DELTA(&lower,51200-50*pi);
    RUN(fixture_box(L"FIELD_MODULE",6,8,4,&module));
    RUN(fixture_component(module,55,55,-12,&window));
    CMD(L"已核对 1 个指定实例",L"ASM_FIELD_WINDOW %ls WINDOW PLANE:ASM_FRONT PLANE:ASM_RIGHT POS ID:%d 1 ID:%d",window.id,a.id);
    a_volume-=640; DELTA(&a,a_volume);
    RUN(ax_layout_token_datum(root,L"CREO_OP DATUM_CURVE TARGET CABLE_PATH CSYS:ASM_CSYS 70 45 -10 70 45 20"));
    CMD(L"已核对 1 个指定实例",L"ASM_FIELD_ROUTE %ls CABLE PLANE:ASM_FRONT PLANE:ASM_RIGHT POS CURVE:CABLE_PATH 4 ID:%d",a.id);
    a_volume-=32*pi; DELTA(&a,a_volume); DELTA(&b,51200);
    swprintf_s(line,_countof(line),L"CREO_OP ASM_FIELD_HOLE %ls MISSES PLANE:ASM_FRONT PLANE:ASM_RIGHT POS 900 900 0 4 ID:%d",asm_name,a.id);
    if(!command(root,line,1,L"已清理")){error=-1;goto done;}
    swprintf_s(line,_countof(line),L"CREO_OP ASM_FIELD_HOLE %ls PARTIAL PLANE:ASM_FRONT PLANE:ASM_RIGHT POS 12 30 0 4 ID:%d ID:%d",asm_name,a.id,third.id);
    if(!command(root,line,1,L"已清理")){error=-1;goto done;}
    DELTA(&a,a_volume);
    DELTA(&third,51200);
    ProFeature absent;
    if(ux_named_feature(root,L"PARTIAL_1",&absent)!=PRO_TK_E_NOT_FOUND){error=-1;goto done;}
    RUN(ProMdlVerstampGet(part,&after));
    if(!ProVerstampEqual(stamp,after)){error=-1;goto done;}
    ProAsmcomppath move_path;
    ProIdTable ids={third.id};
    ProMatrix moved={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    RUN(ProAsmcomppathInit((ProSolid)root,ids,1,&move_path));
    RUN(ProAsmcompPositionSet(&move_path,&third,moved));
    RUN(part_regenerate(root));
    CMD(L"另有 3 个同模型实例",L"ASM_FIELD_AUDIT %ls CUSTOM%ls",L"");
    RUN(ProMdlSave(part));
    RUN(ProMdlSave(source));
    RUN(ProMdlSave(module));
    RUN(ProMdlSave(root));
    int win;
    RUN(ProMdlWindowGet(root,&win));
    int source_window;
    RUN(ProMdlWindowGet(source,&source_window));
    RUN(ProWindowCurrentSet(source_window));
    RUN(ProWindowDelete(win));
    RUN(ProMdlErase(root));
    root=NULL;
    RUN(ProMdlnameRetrieve(asm_name,PRO_MDL_ASSEMBLY,&root));
    RUN(part_regenerate(root));
    ProFeature result;
    RUN(ux_named_feature(root,L"CUSTOM",&result));
    AxFieldTargets expected={0};
    ids[0]=a.id;
    RUN(ProAsmcomppathInit((ProSolid)root,ids,1,&expected.paths[0]));
    expected.count=1;
    RUN(ax_field_check_hits(&result,&expected));
    error=failures?PRO_TK_GENERAL_ERROR:PRO_TK_NO_ERROR;
done:
    if(stamp)ProVerstampFree(&stamp);
    if(after)ProVerstampFree(&after);
    trace("FIELDWORK_OPS_CASES",cases);
    trace("FIELDWORK_OPS_FAILURES",failures);
    trace("FIELDWORK_OPS_DONE",error);
    ProEngineerEnd();
    return 0;
#undef CMD
#undef RUN
}

static int prepare_fieldwork(uiCmdCmdId id,uiCmdValue *value,void *context){
    const char *stage="fieldwork assembly";
    model_background_mode=0;
    ProError error=model_create_empty(3,asm_name,&stage);
    if(!error)error=ProMdlnameInit(asm_name,PRO_MDL_ASSEMBLY,&root);
    if(!error)error=ProMacroLoad(L"~ Command `BWTLFieldworkRun`;");
    if(error){trace("FIELDWORK_OPS_DONE",error);ProEngineerEnd();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLFieldworkPrepare",prepare_fieldwork,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLFieldworkRun",run_fieldwork,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLFieldworkPrepare`;");
}
