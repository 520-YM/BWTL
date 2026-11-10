#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

static ProError inspection_command(ProMdl model,const wchar_t *text,int reject,const wchar_t *expected){
    ProWVerstamp before=NULL,after=NULL;
    ProError error=ProMdlVerstampGet(model,&before);
    ProError result=panel_command(model,text,reject,expected);
    panel_check(!error&&!ProMdlVerstampGet(model,&after)&&ProVerstampEqual(before,after),
        "inspection kept native model version stamp");
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    return result;
}

typedef struct {double radius,x;int id;} InspectionCylinder;
static ProError inspection_find_cylinder(ProSurface face,ProError status,ProAppData data){
    if(status)return status;
    InspectionCylinder *find=data;
    ProSrftype type;
    ProError error=ProSurfaceTypeGet(face,&type);
    if(error||type!=PRO_SRF_CYL)return error;
    ProGeomitemdata *raw=NULL;
    error=ProSurfaceDataGet(face,&raw);
    if(!error){
        ProCylinderdata *c=&raw->data.p_surface_data->srf_shape.cylinder;
        if(part_close(c->radius,find->radius)&&part_close(c->origin[0],find->x))error=ProSurfaceIdGet(face,&find->id);
        ProGeomitemdataFree(&raw);
    }
    return error;
}

static int inspection_cylinder(ProMdl model,double radius,double x){
    ProSolidBody body;
    InspectionCylinder find={radius,x,0};
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,inspection_find_cylinder,&find);
    panel_check(!error&&find.id,"find cylindrical hole wall");
    return find.id;
}

typedef struct {ProSurface face;Inspection view;int bend,id;} InspectionFixtureEdge;
static ProError inspection_fixture_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    InspectionFixtureEdge *search=data;
    int id;
    ProError error=ProEdgeIdGet(edge,&id);
    if(error)return error;
    if(search->bend){
        wchar_t token[32];
        InspectionBend bend;
        int face;
        swprintf_s(token,_countof(token),L"EDGE:%d",id);
        if(!ProSurfaceIdGet(search->face,&face)&&!in_bend_geometry(&search->view,token,&bend)&&
           bend.wall.id==face&&fabs(bend.axis[0])>0.999)search->id=id;
    }else{
        ProVector a,b;
        if(!ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL)&&!ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)&&
           fabs(a[1])<1e-7&&fabs(b[1])<1e-7&&fabs(fabs(a[0]-b[0])-80)<1e-7)search->id=id;
    }
    return PRO_TK_NO_ERROR;
}

static ProError inspection_fixture_contour(ProContour contour,ProError status,ProAppData data){
    InspectionFixtureEdge *search=data;
    return status?status:ProContourEdgeVisit(search->face,contour,inspection_fixture_edge,NULL,search);
}

static int inspection_find_edge(ProMdl model,int face,int bend){
    InspectionFixtureEdge search={0};
    search.view.model=model;search.view.mm=1;
    search.bend=bend;
    ProError error=ProSurfaceInit((ProSolid)model,face,&search.face);
    if(!error)error=ProSurfaceContourVisit(search.face,inspection_fixture_contour,NULL,&search);
    panel_check(!error&&search.id,"find required original edge");
    return search.id;
}

typedef struct {ProMdl model;int first,second;} InspectionSlot;
static ProError inspection_slot(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    InspectionSlot *s=data;
    InspectionPlane plane;
    if(!in_plane_read(s->model,surface,&plane)){
        if(plane.normal[0]>0.999&&fabs(plane.point[0]-30)<1e-6)s->first=plane.id;
        if(plane.normal[0]<-0.999&&fabs(plane.point[0]-50)<1e-6)s->second=plane.id;
    }
    return PRO_TK_NO_ERROR;
}

static ProError inspection_cut(ProMdl model,int face,const wchar_t *name,PanelProfile profile,double depth){
    PartCommand seed={0};
    ShopSetup setup;
    FdTxn tx={0};
    swprintf_s(seed.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",face);
    wcscpy_s(seed.arg[4],PART_TOKEN_MAX,L"CSYS:CHECK_AT");
    wcscpy_s(seed.arg[5],PART_TOKEN_MAX,L"PLANE:RIGHT");
    ProError error=fd_begin(&tx,model,name,PRO_FEAT_CUT);
    if(!error)error=mp_setup(&tx,&seed,3,&setup);
    if(!error)error=mp_blind(&tx,&setup,L"SK",&profile,1,depth);
    fd_cleanup(&tx,0);
    return error;
}

static int inspection_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    Fixture f;
    wchar_t text[1200];
    ProError error;
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define CHECK(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP CHK_" format,__VA_ARGS__),inspection_command(model,text,reject,expected))
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls CHECK_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name); \
    if(error){trace("inspection fixture error",error);panel_check(0,"create inspection fixture");goto done;}}while(0)
#define HOLE(feature,hx,hy,d,mode,deep) CMD(0,NULL,L"HOLE %ls " feature L" SURFACE:%d SURFACE:%d %d SURFACE:%d %d %d " mode L" %d",name,f.top,f.x,hx,f.y,hy,d,deep)
    MAKE();
    HOLE(L"MOUNT",40,30,8,L"THROUGH",0);
    int bore=inspection_cylinder(model,4,40);
    CHECK(0,L"环形支承宽 3 mm",L"SEAT %ls SURFACE:%d SURFACE:%d 14 7 2",name,f.top,bore);
    CHECK(0,L"落位不符合",L"SEAT %ls SURFACE:%d SURFACE:%d 14 9 2",name,f.top,bore);
    CHECK(0,L"头缘到其它面边界净距 -5 mm",L"SEAT %ls SURFACE:%d SURFACE:%d 70 7 2",name,f.top,bore);
    CHECK(0,L"允许最大刀径 7 mm",L"CUTTER %ls CYL SURFACE:%d 6 0.5",name,bore);
    CHECK(0,L"刀具直径筛查不符合",L"CUTTER %ls CYL SURFACE:%d 7.5 0.5",name,bore);
    CHECK(1,L"检查未完成",L"BOTTOM %ls MOUNT 2",name);
    CHECK(0,L"设备安装净空确认",L"DEVICE %ls CSYS:CHECK_AT 40 30 15 12 12 10 1",name);
    CHECK(0,L"实体侵入",L"DEVICE %ls CSYS:CHECK_AT 40 30 -10 12 12 10 0",name);
    CHECK(0,L"需进一步复核",L"DEVICE %ls CSYS:CHECK_AT 40 30 -10 2 2 10 0",name);
    CHECK(0,L"标出 1 条外凸尖角",L"SHARP %ls 95 EDGE:%d",name,f.vertical[0]);
    CHECK(0,L"标出 0 条外凸尖角",L"SHARP %ls 80 EDGE:%d",name,f.vertical[0]);
    MAKE();
    HOLE(L"BLIND_A",40,30,6,L"BLIND",12);
    CHECK(0,L"最薄 8 mm",L"BOTTOM %ls BLIND_A 6",name);
    CHECK(0,L"底厚低于要求",L"BOTTOM %ls BLIND_A 9",name);
    CHECK(0,L"深径比 2，超过",L"RATIO %ls BLIND_A 1.5",name);
    CHECK(0,L"深径比 2，符合",L"RATIO %ls BLIND_A 2.5",name);
    MAKE();
    CMD(0,NULL,L"HOLE_COUNTERBORE %ls BORE_A SURFACE:%d SURFACE:%d 24 SURFACE:%d 25 6 12 4 THROUGH 0",name,f.top,f.x,f.y);
    CMD(0,NULL,L"HOLE_COUNTERSINK %ls SINK_A SURFACE:%d SURFACE:%d 55 SURFACE:%d 30 6 12 90 THROUGH 0",name,f.top,f.x,f.y);
    CHECK(0,L"头顶低于入口面 1 mm",L"HIDE %ls BORE_A BORE 10 3 0 0.5",name);
    CHECK(0,L"藏头不符合",L"HIDE %ls BORE_A BORE 10 5 0 0.5",name);
    CHECK(0,L"头顶低于入口面 1 mm",L"HIDE %ls SINK_A SINK 10 2 90 0.5",name);
    CHECK(0,L"锥头角度",L"HIDE %ls SINK_A SINK 10 2 100 0.5",name);
    MAKE();
    CMD(0,NULL,L"VENT_ROUND %ls AIR SURFACE:%d CSYS:CHECK_AT PLANE:RIGHT 30 30 20 2 2 13 13 8 GRID",name,f.top);
    CMD(0,NULL,L"VENT_SLOT %ls AIR_SLOT SURFACE:%d CSYS:CHECK_AT PLANE:RIGHT 58 30 20 1 1 1 1 14 4",name,f.top);
    error=inspection_cut(model,f.top,L"BLIND_SAMPLE",mp_circle(as_point(56,47),6),5);
    panel_check(!error,"create native blind cavity on vent plate");
    {wchar_t expected[60];
     swprintf_s(expected,_countof(expected),L"直通开口率 %.8g%%",100*(68*AS_PI+40)/4800);
     CHECK(0,expected,L"VENT %ls SURFACE:%d SURFACE:%d 4",name,f.top,f.bottom);
     CHECK(0,L"开孔率待复核",L"VENT %ls SURFACE:%d SURFACE:%d 5.6",name,f.top,f.bottom);
     CHECK(0,L"开孔率低于要求",L"VENT %ls SURFACE:%d SURFACE:%d 8",name,f.top,f.bottom);
     CHECK(1,L"检查未完成",L"VENT %ls SURFACE:%d SURFACE:%d 4",name,f.top,f.xfar);}
    MAKE();
    {error=inspection_cut(model,f.top,L"SLOT_SAMPLE",pn_rect(40,30,20,10),5);
     panel_check(!error,"create native slot sample");
     if(!error){
         InspectionSlot walls={model,0,0};
         error=ProSolidSurfaceVisit((ProSolid)model,inspection_slot,NULL,&walls);
         panel_check(!error&&walls.first&&walls.second,"find opposing slot walls");
         CHECK(0,L"允许最大刀径 18 mm",L"CUTTER %ls SLOT SURFACE:%d SURFACE:%d 12 1",name,walls.first,walls.second);
         CHECK(1,L"检查未完成",L"CUTTER %ls SLOT SURFACE:%d SURFACE:%d 12 1",name,walls.first,f.top);
     }}
    {ModelPlan plate={0,80,60,2,0,1};
     const char *stage="inspection sheet fixture";
     PanelFaceFixture top={NULL,2,0};
     error=model_create(&plate,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     top.model=model;
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls SHEET_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
     if(!error&&top.id)error=CMD(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
     top.id=0;
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     panel_check(!error&&top.id,"create native inspection sheet");
     if(!error&&top.id){
         int edge=inspection_find_edge(model,top.id,0);
         error=CMD(0,NULL,L"SMT_FLANGE %ls FOLD EDGE:%d 14 90 2 POS",name,edge);
         top.id=0;
         if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
         if(!error&&top.id){
             CMD(0,NULL,L"PN_BUTTON %ls SHEET_HOLE SURFACE:%d CSYS:SHEET_AT PLANE:RIGHT 40 30 2 6 0 0",name,top.id);
             int bend=inspection_find_edge(model,top.id,1),hole=inspection_cylinder(model,3,40);
             CHECK(0,L"孔与折弯距离符合",L"BEND %ls SURFACE:%d SURFACE:%d EDGE:%d 4",name,top.id,hole,bend);
             CHECK(0,L"孔与折弯距离低于要求",L"BEND %ls SURFACE:%d SURFACE:%d EDGE:%d 60",name,top.id,hole,bend);
             CHECK(0,L"折弯工具净空确认",L"TOOLSPACE %ls EDGE:%d CSYS:SHEET_AT 40 30 100 20 10 10 2",name,bend);
             CHECK(0,L"折弯工具净空不足",L"TOOLSPACE %ls EDGE:%d CSYS:SHEET_AT 40 16 -1 10 10 1 0",name,bend);
         }else panel_check(0,"locate native flange and base face");
     }}
#undef HOLE
#undef MAKE
#undef CHECK
#undef CMD
done:
    trace("INSPECTION_CASES",cases);
    trace("INSPECTION_FAILURES",failures);
    trace("INSPECTION_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId command;
    ProError error=ProCmdActionAdd("BWTLInspectionProbe",inspection_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLInspectionProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
