#define BWTL_SHOP_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

typedef struct {double radius,x;int id;} ShopCylinderFixture;
static ProError shop_pick_cylinder(ProSurface surface,ProError status,ProAppData data){
    if(status)return status;
    ShopCylinderFixture *fixture=data;
    ProGeomitemdata *raw=NULL;
    ProError error=ProSurfaceDataGet(surface,&raw);
    if(!error){
        ProSurfacedata *s=raw->data.p_surface_data;
        if(s->type==PRO_SRF_CYL){
            ProCylinderdata *c=&s->srf_shape.cylinder;
            if(fabs(c->radius-fixture->radius)<1e-5&&fabs(c->origin[0]-fixture->x)<1e-5)
                error=ProSurfaceIdGet(surface,&fixture->id);
        }
        ProGeomitemdataFree(&raw);
    }
    return error;
}

static int shop_cylinder(ProMdl model,double r,double x){
    ShopCylinderFixture fixture={r,x,0};
    ProSolidBody body;
    ProError error=qa_single_body(model,&body);
    if(!error)error=ProSolidBodySurfaceVisit(&body,shop_pick_cylinder,&fixture);
    panel_check(!error&&fixture.id,"find actual cylinder");
    return fixture.id;
}

static int shop_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    Fixture f;
    ProError error;
    wchar_t text[1200];
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define SHOP(reject,expected,verb,feature,tail) CMD(reject,expected,L"SHOP_" verb L" %ls " feature L" SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT " tail,name,f.top)
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls SHOP_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name); \
    if(error){trace("shop fixture error",error);panel_check(0,"create shop fixture");goto done;}}while(0)
#define VOLUME(expected) panel_check(fabs(volume(model)-(expected))<0.02,"independently verified shop volume")
#define BORE(feature,x) CMD(0,NULL,L"PN_BUTTON %ls " feature L" SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT %d 30 20 8 0 0",name,f.top,x)
    MAKE();
    SHOP(1,NULL,L"STEP",L"BAD_DEPTHS",L"40 30 3 16 5 12 4 8 12");
    SHOP(1,NULL,L"STEP",L"BREAKOUT",L"40 30 3 16 5 12 10 8 22");
    mp_test_fail_after_first_cut=1;
    SHOP(1,L"已核对原实体",L"STEP",L"STEP_ROLLBACK",L"40 30 3 16 4 12 8 8 12");
    mp_test_fail_after_first_cut=0;
    panel_check(mp_test_fault_hit,"rollback injected after first cut");
    VOLUME(96000);
    SHOP(0,L"3 级同轴盲孔",L"STEP",L"STEP_HOLE",L"40 30 3 16 4 12 8 8 12");
    VOLUME(96000-(64+36+16)*4*AS_PI);
    MAKE();
    CMD(0,NULL,L"DATUM_CSYS %ls SIDE_AT CSYS:PRT_CSYS_DEF 80 30 10 0 90 0",name);
    CMD(1,NULL,L"SHOP_CROSS %ls OFF_AXIS SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT 40 32 4 16 SURFACE:%d CSYS:SIDE_AT PLANE:TOP 0 0 4 46",name,f.top,f.xfar);
    CMD(0,L"垂直相通",L"SHOP_CROSS %ls CROSS_PATH SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT 40 30 4 16 SURFACE:%d CSYS:SIDE_AT PLANE:TOP 0 0 4 46",name,f.top,f.xfar);
    VOLUME(96000-4*AS_PI*62+128.0/3);
    MAKE();
    BORE(L"PILOT",40);
    int bore=shop_cylinder(model,4,40);
    CMD(0,NULL,L"DATUM_CSYS %ls BACK_AT CSYS:PRT_CSYS_DEF 40 30 0 180 0 0",name);
    CMD(1,NULL,L"SHOP_BACK %ls LOW_STOCK SURFACE:%d CSYS:BACK_AT PLANE:RIGHT SURFACE:%d 16 12 9",name,f.bottom,bore);
    CMD(0,L"孔背面沉台",L"SHOP_BACK %ls BACK_RECESS SURFACE:%d CSYS:BACK_AT PLANE:RIGHT SURFACE:%d 16 5 5",name,f.bottom,bore);
    VOLUME(96000-16*AS_PI*20-48*AS_PI*5);
    MAKE();
    BORE(L"PILOT",25);
    BORE(L"PILOT_2",55);
    bore=shop_cylinder(model,4,25);
    int other=shop_cylinder(model,4,55);
    CMD(1,NULL,L"SHOP_DEBURR %ls DUPLICATE 1 2 SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d",name,bore,f.top,bore,f.top);
    CMD(0,L"3 个圆孔口",L"SHOP_DEBURR %ls CLEAN_RIMS 1 3 SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d SURFACE:%d",name,bore,f.top,bore,f.bottom,other,f.top);
    VOLUME(96000-32*AS_PI*20-13*AS_PI);
    MAKE();
    SHOP(1,NULL,L"ISLAND",L"OVERLAP_ISLAND",L"40 30 30 24 5 2 38 30 6 6 40 30 6 6");
    SHOP(0,L"保留 2 个矩形岛",L"ISLAND",L"ISLANDS",L"40 30 30 24 5 2 35 30 4 6 46 30 6 4");
    VOLUME(96000-(30*24-48)*5);
    MAKE();
    SHOP(0,L"2 层同心矩形",L"LEVEL",L"LEVELS",L"40 30 2 30 24 4 20 16 9");
    VOLUME(96000-30*24*4-20*16*5);
    MAKE();
    {PartCommand seed={0};
     ShopSetup setup;
     FdTxn tx={0};
     PanelProfile pocket=pn_rect(40,30,30,24);
     swprintf_s(seed.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",f.top);
     wcscpy_s(seed.arg[4],PART_TOKEN_MAX,L"CSYS:SHOP_AT");
     wcscpy_s(seed.arg[5],PART_TOKEN_MAX,L"PLANE:RIGHT");
     error=fd_begin(&tx,model,L"POCKET_SEED",PRO_FEAT_CUT);
     if(!error)error=mp_setup(&tx,&seed,3,&setup);
     if(!error)error=mp_blind(&tx,&setup,L"SK",&pocket,1,5);
     fd_cleanup(&tx,0);
     panel_check(!error,"seed rectangular pocket");
     if(error)goto done;}
    SHOP(1,NULL,L"DOG",L"WRONG_POCKET",L"40 30 30 24 4 4");
    SHOP(0,L"四个内角",L"DOG",L"DOGBONES",L"40 30 30 24 5 4");
    VOLUME(96000-30*24*5-60*AS_PI);
    MAKE();
    BORE(L"PILOT",40);
    bore=shop_cylinder(model,4,40);
    CMD(1,NULL,L"SHOP_FLAT %ls BIG_FLAT SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT SURFACE:%d 8 2 35",name,f.top,bore);
    CMD(0,L"直壁止转缺口",L"SHOP_FLAT %ls LOCK_FLAT SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT SURFACE:%d 4 2 35",name,f.top,bore);
    VOLUME(96000-(16*AS_PI+24-2*sqrt(12.0)-16*asin(0.5))*20);
    {ModelPlan shaft={1,40,40,40,0,1};
     const char *stage="shaft fixture";
     error=model_create(&shaft,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     panel_check(!error,"create solid shaft");
     if(error)goto done;
     int cylinder=shop_cylinder(model,20,20);
     CMD(0,NULL,L"DATUM_CSYS %ls SHAFT_AT CSYS:PRT_CSYS_DEF 20 20 0 0 0 0",name);
     CMD(0,NULL,L"DATUM_POINT %ls SHAFT_ZERO CSYS:SHAFT_AT 0 0 0",name);
     CMD(0,NULL,L"DATUM_POINT %ls SHAFT_END CSYS:SHAFT_AT 0 0 20",name);
     CMD(0,NULL,L"DATUM_POINT %ls SHAFT_X CSYS:SHAFT_AT 10 0 0",name);
     CMD(0,NULL,L"DATUM_AXIS_2POINT %ls SHAFT_AXIS POINT:SHAFT_ZERO POINT:SHAFT_END",name);
     CMD(0,NULL,L"DATUM_PLANE_3POINT %ls SHAFT_PLN POINT:SHAFT_ZERO POINT:SHAFT_END POINT:SHAFT_X",name);
     CMD(1,NULL,L"SHOP_GROOVE %ls AT_END SURFACE:%d CSYS:SHAFT_AT AXIS:SHAFT_AXIS PLANE:SHAFT_PLN PLANE:RIGHT 0 7 3 1",name,cylinder);
     CMD(0,L"槽根圆角 R1",L"SHOP_GROOVE %ls ROOT_GROOVE SURFACE:%d CSYS:SHAFT_AT AXIS:SHAFT_AXIS PLANE:SHAFT_PLN PLANE:RIGHT 12 7 3 1",name,cylinder);
     double corner=17.5-AS_PI/4*(18-4/(3*AS_PI));
     VOLUME(16000*AS_PI-2*AS_PI*(7*3*18.5-2*corner));}
    MAKE();
    BORE(L"PILOT",40);
    bore=shop_cylinder(model,4,40);
    CMD(1,NULL,L"SHOP_TOOL %ls TOO_NARROW SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT SURFACE:%d 7 0 5 5",name,f.top,bore);
    CMD(0,L"直径 16 mm",L"SHOP_TOOL %ls DRIVER_ROOM SURFACE:%d CSYS:SHOP_AT PLANE:RIGHT SURFACE:%d 14 1 5 5",name,f.top,bore);
    VOLUME(96000-16*AS_PI*20-48*AS_PI*5);
#undef BORE
#undef VOLUME
#undef MAKE
#undef SHOP
#undef CMD
done:
    trace("SHOP_CASES",cases);
    trace("SHOP_FAILURES",failures);
    trace("SHOP_DONE",1);
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
    ProError error=ProCmdActionAdd("BWTLShopProbe",shop_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLShopProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
