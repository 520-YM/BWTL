#define BWTL_FASTENER_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

typedef struct {ProSurface face;int id;} FixEdgeFixture;
static ProError fix_left_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    FixEdgeFixture *fixture=data;
    ProEnttype type;
    ProVector a,b,d1,d2,d3;
    if(!ProEdgeTypeGet(edge,&type)&&type==PRO_ENT_LINE&&
       !ProEdgeXyzdataEval(edge,0,a,d1,d2,d3)&&!ProEdgeXyzdataEval(edge,1,b,d1,d2,d3)&&
       fabs(a[0])<1e-6&&fabs(b[0])<1e-6&&fabs(fabs(a[1]-b[1])-60)<1e-6)ProEdgeIdGet(edge,&fixture->id);
    return PRO_TK_NO_ERROR;
}
static ProError fix_left_contour(ProContour contour,ProError status,ProAppData data){
    FixEdgeFixture *fixture=data;
    return status?status:ProContourEdgeVisit(fixture->face,contour,fix_left_edge,NULL,fixture);
}

static int fastening_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    Fixture f;
    ProError error;
    wchar_t text[1200],parameters[320];
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define FIX(reject,expected,verb,feature,tail) CMD(reject,expected,L"FIX_" verb L" %ls " feature L" SURFACE:%d CSYS:FIX_AT PLANE:RIGHT %ls",name,f.top,tail)
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls FIX_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name); \
    if(error){trace("fastener fixture",error);panel_check(0,"create fastening fixture");goto done;}}while(0)
#define VOLUME(delta) panel_check(part_close(volume(model),96000-(delta)),"actual volume matches independent expected cut")
    MAKE();
    FIX(0,L"钥匙孔挂槽",L"KEY",L"KEY_HOOK",L"30 30 20 12 4 14 35");
    {double segment=36*acos(sqrt(32.0)/6)-2*sqrt(32.0);
     VOLUME((36*AS_PI-segment+4*(14-sqrt(32.0))+2*AS_PI)*20);}
    FIX(1,NULL,L"KEY",L"BAD_KEY",L"30 30 20 6 7 14 0");
    MAKE();
    FIX(0,L"槽宽 5 mm，名义调节行程 10 mm",L"SLOT",L"ADJUST",L"40 30 20 4 0.5 10 90");
    VOLUME((50+6.25*AS_PI)*20);
    MAKE();
    FIX(1,L"边距",L"CORNERS",L"BAD_CORNERS",L"40 30 20 CENTER 60 40 4 9");
    FIX(0,L"最小净距 8 mm",L"CORNERS",L"CTR_CORNERS",L"40 30 20 CENTER 60 40 4 7");
    VOLUME(16*AS_PI*20);
    MAKE();
    FIX(1,L"边距",L"CORNERS",L"BAD_PLATE",L"0 0 20 EDGE 79 60 10 8 4 5");
    FIX(0,L"最小净距 6 mm",L"CORNERS",L"EDGE_CORNERS",L"0 0 20 EDGE 80 60 10 8 4 5");
    VOLUME(16*AS_PI*20);
    MAKE();
    CMD(0,NULL,L"PN_BUTTON %ls FLANGE_BORE SURFACE:%d CSYS:FIX_AT PLANE:RIGHT 40 30 20 14 0 0",name,f.top);
    FIX(0,L"6 个贯穿轮廓",L"PCD",L"FLANGE_BOLTS",L"40 30 20 44 8 0 4 2 2 7");
    VOLUME((49+24)*AS_PI*20);
    FIX(1,NULL,L"PCD",L"BAD_SKIP",L"40 30 20 44 8 0 4 2 2 2");
    FIX(1,L"未执行",L"PRESS",L"NEAR_BORE",L"49 30 20 3 19 21 1");
    {ModelPlan ring={1,80,80,20,16,1};const char *stage="native annular flange";
     error=model_create(&ring,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     PanelFaceFixture top={model,20,0};
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error&&!top.id)error=PRO_TK_E_NOT_FOUND;
     if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls FIX_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name);
     panel_check(!error,"create native annular flange and read its planar face");
     if(!error){
         f.top=top.id;
         double original=volume(model);
         FIX(0,L"5 个贯穿轮廓",L"PCD",L"RING_BOLTS",L"40 40 20 50 6 30 4 1 2");
         panel_check(part_close(volume(model),original-20*AS_PI*20),"native annulus verifies bolt count and area");
         PartCommand geometry={0};
         FdTxn context={0};
         PanelFrame frame={0};
         context.model=model;
         context.scale=1;
         swprintf_s(geometry.arg[3],PART_TOKEN_MAX,L"SURFACE:%d",f.top);
         wcscpy_s(geometry.arg[4],PART_TOKEN_MAX,L"CSYS:FIX_AT");
         AsPoint cut={40+25*cos(AS_PI/6),40+25*sin(AS_PI/6)},skipped={40,65};
         panel_check(!pn_frame(&context,&geometry,&frame)&&
             !pn_probe(&context,&frame,cut,0,0,0)&&!pn_probe(&context,&frame,skipped,0,20,0),
             "bolt circle actual phase and skipped second station");
     }}
    MAKE();
    {FixEdgeFixture left={NULL,0};
     if(!ProSurfaceInit((ProSolid)model,f.top,&left.face))ProSurfaceContourVisit(left.face,fix_left_contour,NULL,&left);
     panel_check(left.id>0,"find actual external hinge edge");
     if(left.id){
        swprintf_s(parameters,_countof(parameters),L"16 30 20 EDGE:%d 10 12 3 4 8",left.id);
        FIX(0,L"6 个贯穿轮廓",L"HINGE",L"HINGE",parameters);
        VOLUME(24*AS_PI*20);
     }
     FIX(1,L"未执行",L"HINGE",L"MISSING_EDGE",L"16 30 20 EDGE:999999 10 12 3 4 8");}
    MAKE();
    FIX(1,L"超出输入规格",L"PRESS",L"BAD_GRIP",L"40 30 20 5 1.5 2.5 3");
    FIX(1,L"未执行",L"PRESS",L"BAD_NET",L"3 30 20 5 19 21 1");
    FIX(1,L"未执行",L"PRESS",L"BAD_ACTUAL_T",L"40 30 19 5 18 21 3");
    FIX(0,L"压铆螺母底孔",L"PRESS",L"PRESS_FIT",L"40 30 20 5 19 21 3");
    VOLUME(6.25*AS_PI*20);
    MAKE();
    FIX(0,L"最小净距保守值",L"RIVET",L"RIVET_HEX",L"24 30 20 HEX 8 19 21 4 30");
    FIX(0,L"拉铆螺母底孔",L"RIVET",L"RIVET_ROUND",L"56 30 20 ROUND 6 19 21 4 0");
    VOLUME((32*sqrt(3.0)+9*AS_PI)*20);
    MAKE();
    fx_test_fail_after_cut=1;
    FIX(1,L"已核对移除",L"GROUND",L"MASK_FAIL",L"40 30 20 4 14");
    fx_test_fail_after_cut=0;
    panel_check(fx_test_fault_hit,"injected failure occurred after native cut");
    VOLUME(0);
    FIX(0,L"遮蔽外圈草绘 GROUND_MASK",L"GROUND",L"GROUND",L"40 30 20 4 14");
    VOLUME(4*AS_PI*20);
    MAKE();
    FIX(1,NULL,L"TIE",L"NO_WEB",L"40 30 20 15 3 4 35 2");
    FIX(0,L"中间留料 5 mm",L"TIE",L"TIE_PAIR",L"40 30 20 15 3 8 35 3");
    VOLUME(2*(36+2.25*AS_PI)*20);
    MAKE();
    FIX(1,L"未执行",L"TAB",L"WRONG_EDGE",L"5 30 20 14 1 8 4");
    FIX(0,L"舌宽 12 mm，插入长 8 mm",L"TAB",L"SLIDE_TAB",L"0 30 20 14 1 8 4");
    VOLUME(2*4*8*20);

    {ModelPlan plate={0,80,60,2,0,1};const char *stage="fastening sheet fixture";
     error=model_create(&plate,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     PanelFaceFixture top={model,2,0};
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error&&!top.id)error=PRO_TK_E_NOT_FOUND;
     if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls FIX_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
     if(!error)error=CMD(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
     top.id=0;
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     panel_check(!error&&top.id,"convert native sheet metal and locate outside face");
     if(!error&&top.id){
        f.top=top.id;
        FIX(0,L"钥匙孔挂槽",L"KEY",L"SMT_KEY",L"20 15 2 10 3 9 0");
        FIX(0,L"压铆螺母底孔",L"PRESS",L"SMT_PRESS",L"58 12 2 5 1.5 2.5 3");
        FIX(0,L"拉铆螺母底孔",L"RIVET",L"SMT_HEX",L"58 29 2 HEX 6 1.5 2.5 4 15");
        FIX(0,L"SMT_GROUND_MASK",L"GROUND",L"SMT_GROUND",L"35 46 2 4 10");
        FIX(0,L"中间留料",L"TIE",L"SMT_TIE",L"55 46 2 12 3 8 0 3");
        FIX(0,L"两侧止挡已核对",L"TAB",L"SMT_TAB",L"0 32 2 14 1 8 4");
        ProFeature tab;
        ProFeattype kind;
        panel_check(!ProModelitemByNameInit(model,PRO_FEATURE,L"SMT_TAB",(ProModelitem *)&tab)&&
            !ProFeatureTypeGet(&tab,&kind)&&kind==PRO_FEAT_CUT_SMT,"native normal sheetmetal cut verified");
     }}
#undef VOLUME
#undef MAKE
#undef FIX
#undef CMD
done:
    trace("FASTENER_CASES",cases);
    trace("FASTENER_FAILURES",failures);
    trace("FASTENER_DONE",1);
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
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLFastenerProbe",fastening_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLFastenerProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
