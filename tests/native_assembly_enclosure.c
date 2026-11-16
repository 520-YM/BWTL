#define user_initialize previous_assembly_initialize
#include "native_assembly.c"
#undef user_initialize

typedef struct {ProMdl model;double top;int top_id,bottom_id,side,window[4],edge,shaft,hole[3];} DevFaces;
static ProError env_surface(ProSurface surface,ProError status,ProAppData data){
    DevFaces *f=data;
    if(status)return status;
    ProGeomitem item;
    AxGeometry shape;
    if(ProSurfaceToGeomitem((ProSolid)f->model,surface,&item)||ax_geometry(&item,&shape))return 0;
    if(shape.type==AX_GEOM_PLANE){
        if(shape.direction[2]>0.999&&fabs(shape.point[2]-f->top)<0.001)f->top_id=item.id;
        if(shape.direction[2]<-0.999&&fabs(shape.point[2])<0.001)f->bottom_id=item.id;
        if(shape.direction[0]>0.999&&shape.point[0]>90)f->side=item.id;
        double p[]={20,80,20,60};
        for(int i=0;i<4;i++)if(shape.direction[i/2]*(i%2?-1:1)>0.999&&fabs(shape.point[i/2]-p[i])<0.001)f->window[i]=item.id;
    }else if(shape.type==AX_GEOM_CYLINDER){
        if(fabs(shape.point[0]-15)<0.001&&fabs(shape.point[1]-15)<0.001)f->hole[0]=item.id;
        if(fabs(shape.point[0]-85)<0.001&&fabs(shape.point[1]-15)<0.001)f->hole[1]=item.id;
        if(fabs(shape.point[0]-50)<0.001&&fabs(shape.point[1]-40)<0.001)f->hole[2]=item.id;
        f->shaft=item.id;
    }
    return 0;
}
static ProError env_edge(ProGeomitem *item,ProError status,ProAppData data){
    DevFaces *f=data;
    if(status||f->edge)return status;
    ProEdge edge;
    ProVector a,b,d1,d2,d3;
    if(ProGeomitemToEdge(item,&edge)||ProEdgeXyzdataEval(edge,0,a,d1,d2,d3)||ProEdgeXyzdataEval(edge,1,b,d1,d2,d3))return 0;
    if(fabs(a[2]-f->top)<0.001&&fabs(b[2]-f->top)<0.001&&fabs(a[1]-60)<0.001&&fabs(b[1]-60)<0.001)f->edge=item->id;
    return 0;
}
static ProError env_feature(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProError error=ProFeatureGeomitemVisit(feature,PRO_EDGE,env_edge,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?PRO_TK_NO_ERROR:error;
}
static ProError env_faces(ProMdl model,double t,DevFaces *out){
    out->model=model;out->top=t;
    return ProSolidSurfaceVisit((ProSolid)model,env_surface,NULL,out);
}
static ProError env_stock(ProMdlName name,int cylinder,double w,double d,double t,int holes,ProMdl *model){
    ModelPlan plan={cylinder,w,d,t,holes?30:0,1};
    const char *stage="device fixture stock";
    ProError error=quick_seed(name,model);
    if(!error)error=model_build_internal(&plan,name,&stage,*model,NULL,NULL);
    if(!error&&holes){
        FdTxn tx={0};tx.model=*model;tx.scale=1;
        QuickPlan cut={0};cut.width=w;cut.depth=d;cut.thickness=t;
        quick_opening(&cut,Q_HOLE,15,15,8,8);quick_opening(&cut,Q_HOLE,85,15,8,8);
        error=quick_cuts(&tx,&cut);
    }
    return error;
}
static ProError env_add(ProMdl root,ProMdl model,double x,double y,double z,wchar_t *id){
    ProAsmcomp feature;
    ProMatrix pose={{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}};
    ProError error=ProAsmcompAssemble((ProAssembly)root,(ProSolid)model,pose,&feature);
    if(!error&&!ax_path_label(&feature.id,1,id,32))error=PRO_TK_GENERAL_ERROR;
    return error;
}
static ProError env_datum(ProMdl model,const wchar_t *command){
    PartCommand parsed;
    wchar_t report[PART_TEXT_MAX];
    if(part_command_parse(command,&parsed)!=1)return PRO_TK_BAD_INPUTS;
    return foundation_execute(&parsed,model,report,_countof(report));
}

static ProError invoke_fixture(ProMdl model,const wchar_t *line,int reject,const wchar_t *expected){
    return command(model,line,reject,expected);
}
static int enclosure_run(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl root=NULL,frame=NULL,screen=NULL,glass=NULL,filter=NULL,latch=NULL,plate=NULL,power=NULL,pcb=NULL,fan=NULL;
    ProMdl stud=NULL,cab=NULL,door=NULL,hinge=NULL,module=NULL;
    ProMdlName name=L"";
    wchar_t text[1024],report[PART_TEXT_MAX],frame_id[32],latch_id[32],power_id[32],fan_id[32],stud_a[32],stud_b[32],cab_id[32],door_id[32],floor_id[32],block_id[32];
    const char *stage="device fixture assembly";
    ProError error;
    PartCommand command;
    PartSnapshot snapshot={0};
    int window;
#define SETUP(call) do{if((error=(call))){trace("enclosure setup " #call,error);check(0,"fixture setup");goto done;}}while(0)
#define DO(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),invoke_fixture(root,text,reject,expected))
    if(part_command_parse(L"CREO_OP QUICK_SCREEN EV_FRAME 100 80 10 60 40 80 60 6",&command)!=1){check(0,"window stock protocol");goto done;}
    SETUP(ProDirectoryCurrentGet(snapshot.directory));
    SETUP(quick_execute(&command,&snapshot,report,_countof(report)));
    SETUP(ProMdlnameInit(L"EV_FRAME",PRO_MDL_PART,&frame));
    SETUP(env_stock(L"EV_SCREEN",0,50,30,2,0,&screen));
    SETUP(env_stock(L"EV_GLASS",0,70,50,3,0,&glass));
    SETUP(env_stock(L"EV_FILTER",0,70,50,3,0,&filter));
    SETUP(env_stock(L"EV_LATCH",0,18,12,4,0,&latch));
    SETUP(env_stock(L"EV_PLATE",0,100,80,10,1,&plate));
    SETUP(env_stock(L"EV_POWER",0,100,80,4,1,&power));
    SETUP(env_stock(L"EV_PCB",0,100,80,2,1,&pcb));
    SETUP(env_stock(L"EV_FAN",0,100,80,4,1,&fan));
    SETUP(env_stock(L"EV_STUD",1,6,6,8,0,&stud));
    SETUP(env_stock(L"EV_CAB",0,100,60,10,0,&cab));
    SETUP(env_stock(L"EV_DOOR",0,100,60,10,0,&door));
    SETUP(env_stock(L"EV_HINGE",0,8,12,4,0,&hinge));
    SETUP(env_stock(L"EV_MODULE",0,12,10,5,0,&module));
    SETUP(env_datum(cab,L"CREO_OP DATUM_CSYS EV_CAB H1 CSYS:PRT_CSYS_DEF 20 0 10 0 0 0"));
    SETUP(env_datum(cab,L"CREO_OP DATUM_CSYS EV_CAB H2 CSYS:PRT_CSYS_DEF 60 0 10 0 0 0"));
    SETUP(env_datum(cab,L"CREO_OP DATUM_CSYS EV_CAB RAIL CSYS:PRT_CSYS_DEF 0 40 10 0 0 0"));
    SETUP(env_datum(cab,L"CREO_OP DATUM_PLANE EV_CAB AX_PLANE PLANE:FRONT 10"));
    SETUP(env_datum(door,L"CREO_OP DATUM_PLANE EV_DOOR AX_PLANE PLANE:FRONT -4"));
    SETUP(env_datum(cab,L"CREO_OP DATUM_AXIS EV_CAB PIN PLANES PLANE:TOP PLANE:AX_PLANE"));
    SETUP(env_datum(door,L"CREO_OP DATUM_AXIS EV_DOOR PIN PLANES PLANE:TOP PLANE:AX_PLANE"));
    SETUP(env_datum(hinge,L"CREO_OP DATUM_AXIS EV_HINGE PIN PLANES PLANE:TOP PLANE:FRONT"));
    DevFaces a={0},b={0},c={0},f={0},g={0},p={0},u={0},board={0},v={0},s={0},cabinet={0},d={0},h={0};
    SETUP(env_faces(frame,10,&a));SETUP(env_faces(screen,2,&b));SETUP(env_faces(glass,3,&c));
    SETUP(env_faces(filter,3,&f));SETUP(env_faces(latch,4,&g));SETUP(env_faces(plate,10,&p));
    SETUP(env_faces(power,4,&u));SETUP(env_faces(pcb,2,&board));SETUP(env_faces(fan,4,&v));
    SETUP(env_faces(stud,8,&s));SETUP(env_faces(cab,10,&cabinet));SETUP(env_faces(door,10,&d));SETUP(env_faces(hinge,4,&h));
    SETUP(ProSolidFeatVisit((ProSolid)cab,env_feature,NULL,&cabinet));
    check(a.window[0]&&a.window[1]&&a.window[2]&&a.window[3]&&p.hole[0]&&p.hole[1]&&p.hole[2]&&cabinet.edge,
        "fixture has actual slot walls, holes, and seal edge");
    model_background_mode=0;
    SETUP(model_create_empty(3,name,&stage));
    SETUP(ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&root));
    SETUP(ProMdlWindowGet(root,&window));SETUP(ProWindowCurrentSet(window));
    SETUP(env_add(root,frame,0,0,0,frame_id));
    SETUP(env_add(root,frame,200,0,0,latch_id));
    SETUP(env_add(root,plate,400,0,0,power_id));
    SETUP(env_add(root,plate,600,0,0,fan_id));
    SETUP(env_add(root,plate,800,0,0,floor_id));
    SETUP(env_add(root,stud,812,12,10,stud_a));
    SETUP(env_add(root,stud,882,12,10,stud_b));
    SETUP(env_add(root,cab,1000,0,0,cab_id));
    SETUP(env_add(root,door,1000,0,14,door_id));
    SETUP(env_add(root,door,0,0,25,block_id));
    SETUP(part_regenerate(root));
    DO(0,L"余量 5 mm",L"ASM_SCREEN_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_SCREEN CSYS:PRT_CSYS_DEF SURFACE:%d 8 4",name,frame_id,a.window[0],a.window[1],a.window[2],a.window[3],b.top_id);
    DO(1,L"本次临时实例已清理",L"ASM_GLASS_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_GLASS CSYS:PRT_CSYS_DEF SURFACE:%d 0.2 4",name,frame_id,a.window[0],a.window[1],a.window[2],a.window[3],c.top_id);
    DO(0,L"压边 5 mm",L"ASM_GLASS_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_GLASS CSYS:PRT_CSYS_DEF SURFACE:%d 0.2 4",name,frame_id,a.window[0],a.window[1],a.window[2],a.window[3],c.bottom_id);
    DO(0,L"孔距 70 mm",L"ASM_POWER_MOUNT %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d EV_POWER SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d +X",name,power_id,p.top_id,p.hole[0],p.hole[1],u.bottom_id,u.hole[0],u.hole[1],u.side);
    DO(1,L"方向不一致",L"ASM_POWER_MOUNT %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d EV_POWER SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d -X",name,power_id,p.top_id,p.hole[0],p.hole[1],u.bottom_id,u.hole[0],u.hole[1],u.side);
    DO(0,L"最低实体离指定底面 8 mm",L"ASM_PCB_MOUNT %ls %ls SURFACE:%d SURFACE:%d %ls SURFACE:%d SURFACE:%d EV_PCB SURFACE:%d SURFACE:%d SURFACE:%d %ls SURFACE:%d 7",name,stud_a,s.top_id,s.shaft,stud_b,s.top_id,s.shaft,board.bottom_id,board.hole[0],board.hole[1],floor_id,p.top_id);
    DO(0,L"风口中心偏差 0 mm",L"ASM_FAN_MOUNT %ls %ls SURFACE:%d SURFACE:%d SURFACE:%d EV_FAN SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d +Z SURFACE:%d SURFACE:%d",name,fan_id,p.top_id,p.hole[0],p.hole[1],v.bottom_id,v.hole[0],v.hole[1],v.top_id,p.hole[2],v.hole[2]);
    DO(1,L"包络相交",L"ASM_FILTER_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_FILTER CSYS:PRT_CSYS_DEF SURFACE:%d 13.5 4 +Z 30 %ls",name,frame_id,a.window[0],a.window[1],a.window[2],a.window[3],f.bottom_id,block_id);
    DO(0,L"抽出包络与指定障碍件分离",L"ASM_FILTER_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_FILTER CSYS:PRT_CSYS_DEF SURFACE:%d 13.5 4 +Z 30 %ls",name,frame_id,a.window[0],a.window[1],a.window[2],a.window[3],f.bottom_id,cab_id);
    DO(0,L"2 个独立实例",L"ASM_RAIL_MOUNT %ls %ls CSYS:RAIL EV_MODULE CSYS:PRT_CSYS_DEF 2 10 25",name,cab_id);
    DO(1,L"超出导轨",L"ASM_RAIL_MOUNT %ls %ls CSYS:RAIL EV_MODULE CSYS:PRT_CSYS_DEF 2 90 25",name,cab_id);
    DO(0,L"端面计划插入 5 mm",L"ASM_LATCH_MOUNT %ls %ls CSYS:PRT_CSYS_DEF SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d EV_LATCH CSYS:PRT_CSYS_DEF SURFACE:%d 5 2 3",name,latch_id,a.window[0],a.window[1],a.window[2],a.window[3],g.top_id);
    DO(0,L"2 个铰链的共同转轴",L"ASM_HINGE_MOUNT %ls %ls AXIS:PIN SURFACE:%d %ls AXIS:PIN SURFACE:%d EV_HINGE CSYS:PRT_CSYS_DEF AXIS:PIN SURFACE:%d SURFACE:%d CSYS:H1 CSYS:H2",name,cab_id,cabinet.top_id,door_id,d.bottom_id,h.bottom_id,h.top_id);
    DO(0,L"20% 至 20%",L"ASM_SEAL_CHECK %ls %ls SURFACE:%d %ls SURFACE:%d 5 15 25 EDGE:%d",name,cab_id,cabinet.top_id,door_id,d.bottom_id,cabinet.edge);
    check(!part_health(root)&&!ax_recovery_uncertain,"enclosure remains healthy");
done:
    trace("ENCLOSURE_CASES",cases);trace("ENCLOSURE_FAILURES",failures);trace("ENCLOSURE_DONE",1);
    ProEngineerEnd();
    return 0;
#undef SETUP
#undef DO
}

#ifndef BWTL_REUSE_ENCLOSURE_FIXTURE
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
    ProError error=ProCmdActionAdd("BWTLEnclosureProbe",enclosure_run,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLEnclosureProbe`;");
}
#endif
