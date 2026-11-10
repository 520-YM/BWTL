#define BWTL_REUSE_MACHINING_FIXTURE
#define BWTL_PANEL_FAULT_TEST
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>

static void panel_check(int passed,const char *label){
    cases++;
    if(!passed)failures++;
    trace(label,passed?0:-1);
}

static ProError panel_command(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
    PartCommand parsed;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    FdTxn before={0},after={0};
    double old_volume=volume(model);
    wchar_t report[PART_TEXT_MAX]=L"";
    ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&before);
    ProError error=snapshot?part_snapshot_take_mode(model,snapshot,0):PRO_TK_OUT_OF_MEMORY;
    if(!error){
        if(part_command_parse(protocol,&parsed)!=1)error=PRO_TK_BAD_INPUTS;
        else error=part_execute(&parsed,snapshot,report,_countof(report));
    }
    int pass=(reject?error!=0:error==0)&&(!expected||wcsstr(report,expected));
    if(reject){
        pass=pass&&!ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&after)&&
            before.before_count==after.before_count&&!part_health(model)&&part_close(old_volume,volume(model));
        for(int i=0;pass&&i<after.before_count;i++)if(!fd_was_feature(&before,after.before_ids[i]))pass=0;
    }
    panel_check(pass,"panel protocol and feature transaction");
    FILE *log=_wfopen(L"panel-cases.log",L"a, ccs=UTF-8");
    if(log){fwprintf(log,L"%ls\nRESULT=%d PASS=%d\n%ls\n",protocol,error,pass,report);fclose(log);}
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
    return error;
}

static ProError panel_activate(ProMdl model){
    int window;
    ProError error=ProMdlWindowGet(model,&window);
    return error?error:ProWindowCurrentSet(window);
}

typedef struct {ProMdl model;double z;int id;} PanelFaceFixture;
static ProError panel_pick_top(ProSurface surface,ProError status,ProAppData data){
    PanelFaceFixture *context=data;
    ProGeomitem item;
    ProVector p,n;
    if(status)return status;
    if(!ProSurfaceToGeomitem((ProSolid)context->model,surface,&item)&&!fd_plane_data(&item,p,n)&&
       n[2]>0.999&&fabs(p[2]-context->z)<1e-5)context->id=item.id;
    return PRO_TK_NO_ERROR;
}

static int panel_test(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    Fixture fixture_data;
    ProError error;
    wchar_t text[1024];
    model_background_mode=0;
    if(fixture(&model,name,&fixture_data)){panel_check(0,"native display warmup");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define MAKE() do{error=fixture(&model,name,&fixture_data);if(!error)error=panel_activate(model); \
    if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls PANEL_AT CSYS:PRT_CSYS_DEF 0 0 20 0 0 0",name); \
    if(error){trace("panel fixture error",error);panel_check(0,"native fixture setup");goto done;}}while(0)
#define PANEL(reject,expected,verb,feature,tail) CMD(reject,expected,L"PN_" verb L" %ls " feature L" SURFACE:%d CSYS:PANEL_AT PLANE:RIGHT " tail,name,fixture_data.top)
#define VOLUME(delta) do{panel_check(part_close(volume(model),96000-(delta)),"actual solid volume matches independent expectation");}while(0)
    MAKE();
    PANEL(1,L"未执行",L"BUTTON",L"WRONG_THICK",L"40 30 19 12 0 0");
    PANEL(1,L"未执行",L"BUTTON",L"OFF_EDGE",L"78 30 20 12 0 0");
    PANEL(0,L"按钮开孔",L"BUTTON",L"BTN",L"40 30 20 12 0 0");
    VOLUME(36*AS_PI*20);
    PANEL(1,NULL,L"BUTTON",L"BTN",L"20 20 20 8 0 0");
    PANEL(1,L"未执行",L"BUTTON",L"ON_EXISTING",L"40 30 20 12 0 0");
    MAKE();
    PANEL(0,L"急停按钮开孔",L"ESTOP",L"ESTOP",L"40 30 20 14 4 2");
    VOLUME((49*(AS_PI-asin(2.0/7))+2*sqrt(45.0)+(9-sqrt(45.0))*4)*20);
    MAKE();
    PANEL(1,L"轮廓相交",L"POWER",L"BAD_SOCKET",L"40 30 20 20 12 14 4");
    PANEL(0,L"3 个贯穿轮廓",L"POWER",L"SOCKET",L"40 30 20 20 12 32 4");
    VOLUME((240+8*AS_PI)*20);
    MAKE();
    PANEL(0,L"接口开孔",L"PORT",L"PORT",L"40 30 20 18 10 2");
    VOLUME((180-(4-AS_PI)*4)*20);
    MAKE();
    PANEL(0,L"5 个贯穿轮廓",L"FAN",L"FAN",L"40 30 20 20 26 26 4");
    VOLUME(116*AS_PI*20);
    MAKE();
    PANEL(1,NULL,L"DISPLAY",L"BAD_SCREEN",L"40 30 20 20 12 24 24 5");
    PANEL(1,L"未执行",L"DISPLAY",L"BEZEL_OFF_EDGE",L"12 30 20 20 12 30 24 5");
    PANEL(0,L"单侧遮边 X 5 mm，Y 6 mm",L"DISPLAY",L"SCREEN",L"40 30 20 20 12 30 24 5");
    VOLUME(240*20);
    MAKE();
    pn_test_fail_after_through=1;
    PANEL(1,L"已核对移除",L"SEAT",L"ROLLBACK",L"40 30 20 20 10 30 20 3");
    pn_test_fail_after_through=0;
    VOLUME(0);
    PANEL(0,L"台阶剩余厚度 17 mm",L"SEAT",L"GLASS",L"40 30 20 20 10 30 20 3");
    VOLUME(20*10*20+(30*20-20*10)*3);
    MAKE();
    PANEL(0,L"2 个贯穿轮廓",L"HANDLE",L"HANDLE2",L"40 15 20 28 4 0");
    PANEL(0,L"4 个贯穿轮廓",L"HANDLE",L"HANDLE4",L"40 42 20 28 4 12");
    VOLUME(24*AS_PI*20);
    MAKE();
    PANEL(0,L"3 个贯穿轮廓",L"LOCK",L"LOCK",L"40 30 20 16 5 30 4");
    VOLUME((64*(AS_PI-acos(5.0/8))+5*sqrt(39.0)+8*AS_PI)*20);
    MAKE();
    PANEL(1,L"未执行",L"COMB",L"NOT_EDGE",L"5 30 20 3 4 8 6");
    PANEL(0,L"3 个贯穿轮廓",L"COMB",L"WIRING",L"0 30 20 3 4 8 6");
    VOLUME(3*4*6*20);
    MAKE();
    CMD(0,NULL,L"DATUM_CSYS %ls ROTATED CSYS:PRT_CSYS_DEF 40 30 20 0 0 90",name);
    CMD(0,L"按钮开孔",L"PN_BUTTON %ls ROTATED_KEY SURFACE:%d CSYS:ROTATED PLANE:RIGHT 0 0 20 10 2 1",name,fixture_data.top);
    VOLUME((25*(AS_PI-asin(0.2))+sqrt(24.0)+(6-sqrt(24.0))*2)*20);
    {ModelPlan plate={0,80,60,2,0,1};const char *stage="sheet panel";
     error=model_create(&plate,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     PanelFaceFixture top={model,2,0};
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error&&!top.id)error=PRO_TK_E_NOT_FOUND;
     if(!error)error=CMD(0,NULL,L"DATUM_CSYS %ls PANEL_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
     if(!error)error=CMD(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
     top.id=0;
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     panel_check(!error&&top.id,"convert real sheet metal plate and find outside face");
     if(!error&&top.id){
         fixture_data.top=top.id;
         PANEL(1,L"普通实体板件",L"SEAT",L"SMT_SEAT",L"40 30 2 20 10 30 20 1");
         PANEL(0,L"5 个贯穿轮廓",L"FAN",L"SMT_FAN",L"40 30 2 20 26 26 4");
         panel_check(part_close(volume(model),9600-116*AS_PI*2),"sheet metal fan cut exact volume");
         ProFeature fan;
         ProFeattype type;
         panel_check(!ProModelitemByNameInit(model,PRO_FEATURE,L"SMT_FAN",(ProModelitem *)&fan)&&
             !ProFeatureTypeGet(&fan,&type)&&type==PRO_FEAT_CUT_SMT,"native sheet metal cut type");
         PANEL(0,L"接口开孔",L"PORT",L"SMT_PORT",L"15 15 2 10 6 1");
         PANEL(0,L"3 个贯穿轮廓",L"COMB",L"SMT_COMB",L"0 40 2 3 4 8 6");
         PANEL(0,L"急停按钮开孔",L"ESTOP",L"SMT_STOP",L"62 10 2 8 2 1");
         double stop_area=16*(AS_PI-asin(0.25))+sqrt(15.0)+(5-sqrt(15.0))*2;
         panel_check(part_close(volume(model),9600-(116*AS_PI+60-(4-AS_PI)+72+stop_area)*2),
             "sheet metal round rectangle, edge slots and keyed hole preserve predicted volume");
     }}
#undef VOLUME
#undef PANEL
#undef MAKE
#undef CMD
done:
    trace("PANEL_CASES",cases);
    trace("PANEL_FAILURES",failures);
    trace("PANEL_DONE",1);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))
        return PRO_TK_BAD_CONTEXT;
    HMODULE modules[2048];
    DWORD bytes;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))
        return PRO_TK_GENERAL_ERROR;
    for(size_t i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(modules[i],path,MAX_PATH);
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident")||wcsstr(path,L"creosafe"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    uiCmdCmdId id;
    ProError error=ProCmdActionAdd("BWTLPanelProbe",panel_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLPanelProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
