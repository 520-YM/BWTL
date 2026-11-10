#define BWTL_VENT_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

typedef struct {ProSurface face;double z;int id;} VentEdgeFixture;
static ProError vent_find_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    VentEdgeFixture *f=data;
    ProEnttype type;
    ProVector a,b;
    if(!ProEdgeTypeGet(edge,&type)&&type==PRO_ENT_LINE&&
       !ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL)&&!ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL)&&
       fabs(a[1])<1e-6&&fabs(b[1])<1e-6&&fabs(a[2]-f->z)<1e-6&&fabs(b[2]-f->z)<1e-6&&
       fabs(fabs(a[0]-b[0])-80)<1e-6)return ProEdgeIdGet(edge,&f->id);
    return PRO_TK_NO_ERROR;
}
static ProError vent_find_contour(ProContour contour,ProError status,ProAppData data){
    VentEdgeFixture *f=data;
    return status?status:ProContourEdgeVisit(f->face,contour,vent_find_edge,NULL,f);
}

static int vent_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    Fixture f;
    ProError error;
    wchar_t text[1200];
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define VENT(reject,expected,verb,feature,tail) CMD(reject,expected,L"VENT_" verb L" %ls " feature L" SURFACE:%d CSYS:VENT_AT PLANE:RIGHT " tail,name,f.top)
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls VENT_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name); \
    if(error){trace("vent fixture error",error);panel_check(0,"create ventilation fixture");goto done;}}while(0)
#define VOLUME(expected) panel_check(fabs(volume(model)-(expected))<0.03,"independently verified ventilation volume")
    MAKE();
    VENT(1,NULL,L"ROUND",L"TOO_CLOSE",L"40 30 20 3 4 5 10 5 GRID");
    VENT(0,L"12 个贯穿轮廓",L"ROUND",L"VENT_CIRCLES",L"40 30 20 3 4 10 8 5 STAGGER");
    VOLUME(96000-12*6.25*AS_PI*20);
    MAKE();
    VENT(0,L"64 个贯穿轮廓",L"ROUND",L"VENT_LIMIT",L"40 30 20 8 8 7 6 3 GRID");
    VOLUME(96000-64*2.25*AS_PI*20);
    MAKE();
    VENT(0,L"6 个贯穿轮廓",L"SLOT",L"SLOT_ROW",L"40 30 20 2 3 17 12 14 4");
    VOLUME(96000-6*(40+4*AS_PI)*20);
    MAKE();
    vt_test_fail_after_openings=1;
    VENT(1,L"已核对原实体",L"LOUVER",L"LOUVER_ROLLBACK",L"40 30 20 2 20 8 20 30 1 2");
    vt_test_fail_after_openings=0;
    panel_check(vt_test_fault_hit,"injected failure after actual louver openings");
    VOLUME(96000);
    VENT(0,L"2 片一体斜叶",L"LOUVER",L"LOUVERS",L"40 30 20 2 20 8 20 30 1 2");
    VOLUME(96000-2*20*(8*20-8));
    MAKE();
    VENT(0,L"12 个贯穿轮廓",L"GRID",L"PUNCHED_GRID",L"40 30 20 4 3 10 9 5 4");
    VOLUME(96000-12*5*4*20);
    MAKE();
    {VentEdgeFixture edge={NULL,20,0};
     if(!ProSurfaceInit((ProSolid)model,f.top,&edge.face))ProSurfaceContourVisit(edge.face,vent_find_contour,NULL,&edge);
     panel_check(edge.id>0,"find native lower edge");
     CMD(1,NULL,L"VENT_DRAIN %ls OUTSIDE_EDGE SURFACE:%d CSYS:VENT_AT PLANE:RIGHT 5 2 20 EDGE:%d 14 4",name,f.top,edge.id);
     CMD(0,L"距所选底边 2 mm",L"VENT_DRAIN %ls DRAIN SURFACE:%d CSYS:VENT_AT PLANE:RIGHT 40 2 20 EDGE:%d 14 4",name,f.top,edge.id);
     VOLUME(96000-(40+4*AS_PI)*20);}
    MAKE();
    VENT(1,NULL,L"GLAND",L"GRIP_LIMIT",L"25 30 20 D 10 3 1 2");
    VENT(0,L"配件允许范围",L"GLAND",L"D_GLAND",L"25 30 20 D 10 3 18 22");
    VENT(0,L"配件允许范围",L"GLAND",L"ROUND_GLAND",L"55 30 20 ROUND 10 0 18 22");
    VOLUME(96000-(25*AS_PI+25*(AS_PI-acos(0.6))+12)*20);
    MAKE();
    VENT(0,L"配件允许范围",L"GROMMET",L"GROMMET_RECT",L"25 30 20 RECT 14 10 2 18 22");
    VENT(0,L"配件允许范围",L"GROMMET",L"GROMMET_ROUND",L"55 30 20 ROUND 12 0 0 18 22");
    VOLUME(96000-(140-(4-AS_PI)*4+36*AS_PI)*20);
    MAKE();
    VENT(0,L"实测高度 3 mm",L"STOP",L"WATER_STOP",L"40 30 20 4 30 3");
    VOLUME(96000+4*30*3);
    MAKE();
    VENT(1,NULL,L"SEAL",L"THIN_BOTTOM",L"40 30 20 34 26 3 18 4");
    VENT(0,L"留料 17 mm",L"SEAL",L"RECT_SEAL",L"40 30 20 34 26 3 3 4");
    VOLUME(96000-(34*26-28*20)*3);
    MAKE();
    {ProWVerstamp before=NULL,after=NULL;
     error=ProMdlVerstampGet(model,&before);
     CMD(0,L"净距下界",L"VENT_ROUTE %ls CSYS:VENT_AT 40 30 10 12 4 1",name);
     CMD(0,L"中心线与当前零件实体相交",L"VENT_ROUTE %ls CSYS:VENT_AT 40 30 -5 12 4 1",name);
     CMD(0,L"需进一步核对",L"VENT_ROUTE %ls CSYS:VENT_AT 40 30 2 12 4 1",name);
     panel_check(!error&&!ProMdlVerstampGet(model,&after)&&ProVerstampEqual(before,after),"read-only bend inspection kept version stamp");
     if(before)ProVerstampFree(&before);
     if(after)ProVerstampFree(&after);
     VOLUME(96000);}
    {ModelPlan sheet={0,80,60,2,0,1};
     const char *stage="sheet ventilation fixture";
     error=model_create(&sheet,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     PanelFaceFixture top={model,2,0};
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error&&!top.id)error=PRO_TK_E_NOT_FOUND;
     if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls VENT_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
     if(!error)error=CMD(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
     top.id=0;
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     panel_check(!error&&top.id,"create native sheet and find outer face");
     if(!error&&top.id){
         f.top=top.id;
         VENT(1,L"普通单实体",L"LOUVER",L"SHEET_LOUVER",L"40 30 2 2 20 8 20 30 1 2");
         VENT(0,L"6 个贯穿轮廓",L"ROUND",L"SHEET_ROUND",L"17 13 2 2 3 6 6 4 GRID");
         VENT(0,L"4 个贯穿轮廓",L"SLOT",L"SHEET_SLOT",L"53 13 2 2 2 12 7 9 3");
         VENT(0,L"4 个贯穿轮廓",L"GRID",L"SHEET_GRID",L"15 37 2 2 2 9 8 5 4");
         VENT(0,L"配件允许范围",L"GLAND",L"SHEET_GLAND",L"40 40 2 ROUND 8 0 1.5 2.5");
         VENT(0,L"配件允许范围",L"GROMMET",L"SHEET_GROMMET",L"62 40 2 RECT 12 8 2 1.5 2.5");
         VentEdgeFixture edge={NULL,2,0};
         if(!ProSurfaceInit((ProSolid)model,f.top,&edge.face))ProSurfaceContourVisit(edge.face,vent_find_contour,NULL,&edge);
         panel_check(edge.id>0,"find native sheet lower edge");
         CMD(0,L"距所选底边 1 mm",L"VENT_DRAIN %ls SHEET_DRAIN SURFACE:%d CSYS:VENT_AT PLANE:RIGHT 40 1 2 EDGE:%d 12 3",name,f.top,edge.id);
         VOLUME(9600-2*(24*AS_PI+4*(18+2.25*AS_PI)+80+16*AS_PI+96-(4-AS_PI)*4+27+2.25*AS_PI));
     }}
#undef VOLUME
#undef MAKE
#undef VENT
#undef CMD
done:
    trace("VENT_CASES",cases);
    trace("VENT_FAILURES",failures);
    trace("VENT_DONE",1);
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
    ProError error=ProCmdActionAdd("BWTLVentProbe",vent_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLVentProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
