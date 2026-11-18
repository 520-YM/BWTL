#define BWTL_PLACEMENT_FAULT_TEST
#define user_initialize prior_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

static ProError placement_part(ProMdlName label,int cylinder,double x,double y,double t,double bore,ProMdl *model){
    ModelPlan stock={cylinder,x,y,t,bore,1};
    const char *phase="placement fixture stock";
    ProError error=quick_seed(label,model);
    return error?error:model_build_internal(&stock,label,&phase,*model,NULL,NULL);
}

typedef struct {ProMdl model;int cylinder,top,bottom;} PlacementFaces;

static ProError placement_faces(ProSurface face,ProError status,ProAppData context){
    PlacementFaces *f=context;
    if(status)return status;
    ProGeomitem item;
    AxGeometry shape;
    if(ProSurfaceToGeomitem((ProSolid)f->model,face,&item)||ax_geometry(&item,&shape))return PRO_TK_NO_ERROR;
    if(shape.type==AX_GEOM_CYLINDER)f->cylinder=item.id;
    if(shape.type==AX_GEOM_PLANE&&shape.direction[2]>0.999)f->top=item.id;
    if(shape.type==AX_GEOM_PLANE&&shape.direction[2]<-0.999)f->bottom=item.id;
    return PRO_TK_NO_ERROR;
}

static int placement_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl base=NULL,bracket=NULL,bolt=NULL;
    ProMdlName root_name=L"PLACEMENT_FIXTURE",base_name=L"TEST_BASE",bracket_name=L"TEST_BRACKET",bolt_name=L"TEST_BOLT";
    const char *stage="placement setup";
    ProMdl root=NULL;
    ProAsmcomp bottom={0},same[6]={0},fastener[3]={0};
    ProMatrix before,after;
    double mm;
    int window;
    wchar_t text[800],base_id[32],ids[6][32],bolt_ids[3][32];
    ProError error;
#define SETUP(call) do{if((error=(call))){trace("placement setup " #call,error);check(0,"setup");goto done;}}while(0)
#define PLACE(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),command(root,text,reject,expected))
    SETUP(placement_part(base_name,0,100,100,10,20,&base));
    SETUP(placement_part(bracket_name,0,30,25,5,0,&bracket));
    SETUP(placement_part(bolt_name,1,12,12,20,0,&bolt));
    PlacementFaces base_faces={base},part_faces={bracket},bolt_faces={bolt};
    SETUP(ProSolidSurfaceVisit((ProSolid)base,placement_faces,NULL,&base_faces));
    SETUP(ProSolidSurfaceVisit((ProSolid)bracket,placement_faces,NULL,&part_faces));
    SETUP(ProSolidSurfaceVisit((ProSolid)bolt,placement_faces,NULL,&bolt_faces));
    check(base_faces.top&&base_faces.cylinder&&part_faces.bottom&&bolt_faces.cylinder&&bolt_faces.bottom,"base and fastener fixture faces");
    swprintf_s(text,_countof(text),L"CREO_OP DATUM_AXIS %ls TEST_HOLE_AXIS CYLINDER SURFACE:%d",base_name,base_faces.cylinder);
    SETUP(run(base,text,0));
    swprintf_s(text,_countof(text),L"CREO_OP DATUM_AXIS %ls TEST_BOLT_AXIS CYLINDER SURFACE:%d",bolt_name,bolt_faces.cylinder);
    SETUP(run(bolt,text,0));
    swprintf_s(text,_countof(text),L"CREO_OP DATUM_AXIS %ls TEST_ANGLE PLANES PLANE:RIGHT PLANE:TOP",base_name);
    SETUP(run(base,text,0));
    swprintf_s(text,_countof(text),L"CREO_OP DATUM_AXIS %ls TEST_ANGLE PLANES PLANE:RIGHT PLANE:TOP",bracket_name);
    SETUP(run(bracket,text,0));
    model_background_mode=0;
    SETUP(model_create_empty(3,root_name,&stage));
    SETUP(ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&root));
    SETUP(ProMdlWindowGet(root,&window));
    SETUP(ProWindowCurrentSet(window));
    SETUP(qa_mm_per_unit(root,&mm));
    SETUP(add_component(root,base,0,&bottom));
    ax_path_label(&bottom.id,1,base_id,_countof(base_id));
    for(int i=0;i<6;i++){
        ProMatrix initial={{1,0,0,0},{0,1,0,0},{0,0,1,0},{20.0*i,15.0*i,20,1}};
        SETUP(ProAsmcompAssemble((ProAssembly)root,(ProSolid)bracket,initial,&same[i]));
        ax_path_label(&same[i].id,1,ids[i],_countof(ids[i]));
    }
    for(int i=0;i<3;i++){
        SETUP(add_component(root,bolt,30.0+5*i,&fastener[i]));
        ax_path_label(&fastener[i].id,1,bolt_ids[i],_countof(bolt_ids[i]));
    }
    SETUP(part_regenerate(root));
    PLACE(0,L"孔壁",L"ASM_REFS %ls %ls",root_name,base_id);
    PLACE(0,L"AXIS:TEST_HOLE_AXIS",L"ASM_REFS %ls %ls",root_name,base_id);
    PLACE(0,L"平面",L"ASM_REFS %ls ROOT",root_name);
    PLACE(0,L"定位",L"ASM_PLACEMENT %ls %ls",root_name,ids[0]);
    SETUP(ax_actual_pose(root,&bottom,before));
    PLACE(0,L"已完成固定",L"ASM_FIX %ls %ls",root_name,base_id);
    SETUP(ax_actual_pose(root,&bottom,after));
    check(ax_same_pose(before,after,mm),"fix preserves the base pose");
    SETUP(ax_actual_pose(root,&same[1],before));
    PLACE(0,L"已完成平面贴合",L"ASM_MATE %ls %ls %ls SURFACE:%d SURFACE:%d",root_name,ids[0],base_id,base_faces.top,part_faces.bottom);
    SETUP(ax_actual_pose(root,&same[1],after));
    check(ax_same_pose(before,after,mm),"same-model sibling untouched");
    PLACE(0,L"已完成平面平行",L"ASM_PARALLEL %ls %ls ROOT PLANE:ASM_RIGHT PLANE:RIGHT",root_name,ids[1]);
    PLACE(0,L"已完成平面偏距",L"ASM_OFFSET %ls %ls ROOT PLANE:ASM_FRONT PLANE:FRONT 15",root_name,ids[1]);
    PLACE(1,L"旧值不符",L"ASM_EDIT_OFFSET %ls %ls 2 14 18",root_name,ids[1]);
    PLACE(0,L"已完成定位偏距修改",L"ASM_EDIT_OFFSET %ls %ls 2 15 18",root_name,ids[1]);
    PLACE(0,L"偏距 18 mm",L"ASM_PLACEMENT %ls %ls",root_name,ids[1]);
    SETUP(ax_actual_pose(root,&same[1],after));
    check(fabs(after[3][2]*mm-18)<0.001,"positive offset places component in signed direction");
    memcpy(before,after,sizeof(before));
    ax_placement_fault=1;
    PLACE(1,L"原有约束和组件位置已恢复",L"ASM_EDIT_OFFSET %ls %ls 2 18 22",root_name,ids[1]);
    ax_placement_fault=0;
    SETUP(ax_actual_pose(root,&same[1],after));
    check(ax_same_pose(before,after,mm),"post-write failure restores constrained component pose");
    PLACE(0,L"偏距 18 mm",L"ASM_PLACEMENT %ls %ls",root_name,ids[1]);
    PLACE(0,L"已完成三面定位",L"ASM_THREE %ls %ls ROOT PLANE:ASM_RIGHT PLANE:RIGHT PLANE:ASM_TOP PLANE:TOP PLANE:ASM_FRONT PLANE:FRONT",root_name,ids[2]);
    PLACE(1,NULL,L"ASM_THREE %ls %ls ROOT PLANE:ASM_RIGHT PLANE:RIGHT PLANE:ASM_RIGHT PLANE:TOP PLANE:ASM_FRONT PLANE:FRONT",root_name,ids[3]);
    PLACE(1,NULL,L"ASM_MATE %ls %ls %ls PLANE:FRONT PLANE:FRONT",root_name,ids[3],ids[3]);
    SETUP(ax_actual_pose(root,&same[3],before));
    ax_placement_fault=1;
    PLACE(1,L"原有约束和组件位置已恢复",L"ASM_OFFSET %ls %ls ROOT PLANE:ASM_FRONT PLANE:FRONT 25",root_name,ids[3]);
    ax_placement_fault=0;
    SETUP(ax_actual_pose(root,&same[3],after));
    check(ax_same_pose(before,after,mm)&&!ax_packaged(&same[3]),"post-write failure restores a packaged occurrence");
    PLACE(0,L"已完成轴孔同轴",L"ASM_COAX %ls %ls %ls SURFACE:%d SURFACE:%d",root_name,bolt_ids[0],base_id,base_faces.cylinder,bolt_faces.cylinder);
    PLACE(0,L"已完成轴孔同轴",L"ASM_COAX %ls %ls %ls AXIS:TEST_HOLE_AXIS AXIS:TEST_BOLT_AXIS",root_name,bolt_ids[2],base_id);
    PLACE(0,L"已完成紧固件定位",L"ASM_FASTENER %ls %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d",
        root_name,bolt_ids[1],base_id,base_faces.cylinder,bolt_faces.cylinder,base_faces.top,bolt_faces.bottom);
    SETUP(ax_actual_pose(root,&same[4],before));
    PLACE(0,NULL,L"ASM_OFFSET %ls %ls ROOT PLANE:ASM_FRONT PLANE:FRONT -12",root_name,ids[4]);
    SETUP(ax_actual_pose(root,&same[4],after));
    check(fabs(after[3][2]*mm+12)<0.001,"negative offset places component in signed direction");
    PLACE(0,L"已完成平面角度定位",L"ASM_PLANE_ANGLE %ls %ls %ls AXIS:TEST_ANGLE AXIS:TEST_ANGLE PLANE:TOP PLANE:TOP 35",root_name,ids[5],base_id);
    PLACE(0,L"角度 35°",L"ASM_PLACEMENT %ls %ls",root_name,ids[5]);
    check(!ax_recovery_uncertain,"no unverified recovery");
    check(!part_health(root),"healthy final native assembly");
done:
    trace("PLACEMENT_CASES",cases);
    trace("PLACEMENT_FAILURES",failures);
    trace("PLACEMENT_DONE",1);
    ProEngineerEnd();
    return 0;
#undef PLACE
#undef SETUP
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH]=L"";
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLPlacementProbe",placement_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLPlacementProbe`;");
}
