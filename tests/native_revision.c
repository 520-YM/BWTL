#define BWTL_REVISION_FAULT_TEST
#define user_initialize panel_fixture_initialize
#define user_terminate panel_fixture_terminate
#include "native_panel.c"
#undef user_initialize
#undef user_terminate

typedef struct {int owner;double target;ProDimensiontype type;ProName name;int hits;} RevisionFixtureDim;
static ProError revision_find_dim(ProDimension *dim,ProError status,ProAppData data){
    if(status)return status;
    RevisionFixtureDim *find=data;
    ProFeature owner;
    double value;
    ProDimensiontype type;
    if(!ProDimensionOwnerfeatureGet(dim,&owner)&&(!find->owner||owner.id==find->owner)&&
       !ProDimensionValueGet(dim,&value)&&part_close(value,find->target)&&
       !ProDimensionTypeGet(dim,&type)&&type==find->type){
        find->hits++;
        return ProDimensionSymbolGet(dim,find->name);
    }
    return PRO_TK_NO_ERROR;
}

static int revision_dim(ProMdl model,const wchar_t *feature,double target,ProDimensiontype type,ProName name){
    RevisionFixtureDim find={0,target,type};
    ProFeature f;
    if(feature&&ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)feature,(ProModelitem *)&f))return 0;
    if(feature)find.owner=f.id;
    ProError error=ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,revision_find_dim,NULL,&find);
    if(!error&&find.hits==1)wcscpy_s(name,PRO_NAME_SIZE,find.name);
    return !error&&find.hits==1;
}

static int revision_test(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    ProName x,y,z;
    Fixture f;
    ProError error;
    wchar_t text[1200];
    model_background_mode=0;
    if(fixture(&model,name,&f)){panel_check(0,"warm up Creo model window");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),panel_command(model,text,reject,expected))
#define MAKE() do{error=fixture(&model,name,&f);if(!error)error=panel_activate(model); \
    if(error){trace("revision fixture error",error);panel_check(0,"create revision fixture");goto done;}}while(0)
#define HOLE(feature,a,b,d,mode,deep) CMD(0,NULL,L"HOLE %ls " feature L" SURFACE:%d SURFACE:%d %d SURFACE:%d %d %d " mode L" %d",name,f.top,f.x,a,f.y,b,d,deep)
#define VOLUME(expected) panel_check(fabs(volume(model)-(expected))<0.03,"independently verified revised volume")
    MAKE();
    if(revision_dim(model,NULL,80,PRODIMTYPE_LINEAR,x)&&revision_dim(model,NULL,60,PRODIMTYPE_LINEAR,y)&&
       revision_dim(model,NULL,20,PRODIMTYPE_LINEAR,z)){
        CMD(1,L"恢复",L"REV_SIZE %ls 80 60 20 88 62 20 1 %ls 80 87",name,x);
        VOLUME(96000);
        CMD(0,L"88 × 62 × 21 mm",L"REV_SIZE %ls 80 60 20 88 62 21 3 %ls 80 88 %ls 60 62 %ls 20 21",name,x,y,z);
        VOLUME(88*62*21);
    }else panel_check(0,"find native box driving dimensions");
    MAKE();
    HOLE(L"A",20,20,6,L"THROUGH",0);
    HOLE(L"B",55,20,6,L"THROUGH",0);
    HOLE(L"C",20,42,6,L"THROUGH",0);
    HOLE(L"D",55,42,6,L"THROUGH",0);
    rv_test_after_first=1;
    CMD(1,L"恢复",L"REV_SHIFT %ls 2 3 4 A B C D",name);
    rv_test_after_first=0;
    panel_check(rv_test_fault_hit,"injected failure after first native dimension write");
    CMD(0,L"平移 4 个",L"REV_SHIFT %ls 2 3 4 A B C D",name);
    CMD(0,L"新孔距 30 × 20 mm",L"REV_PITCH %ls 30 20 A B C D",name);
    VOLUME(96000-4*9*AS_PI*20);
    CMD(1,NULL,L"REV_DIAM %ls 5 8 4 A B C D",name);
    CMD(0,L"4 个同规格直孔",L"REV_DIAM %ls 6 8 4 A B C D",name);
    VOLUME(96000-4*16*AS_PI*20);
    MAKE();
    CMD(0,NULL,L"SKETCH_CREATE %ls OPEN_SK SURFACE:%d PLANE:RIGHT CIRCLE 30 30 5",name,f.top);
    {FdSketch sk={0};int *ids=NULL,count=0,circle=-1;ProMatrix placement;
     if(!fd_sketch_open(model,L"OPEN_SK",&sk,0)&&!ProSectionLocationGet(sk.section,placement)&&!ProSectionEntityIdsGet(sk.section,&ids,&count))
         for(int i=0;i<count;i++){Pro2dEntdef *ent;if(!ProSectionEntityGet(sk.section,ids[i],&ent)&&ent->type==PRO_2D_CIRCLE)circle=ids[i];}
     if(ids)ProArrayFree((ProArray *)&ids);
     fd_sketch_free(&sk);
     if(circle>=0){
         CMD(0,NULL,L"SKETCH_DIM %ls OPEN_SK DIAMETER %d NONE 10",name,circle);
         CMD(0,NULL,L"SOLID_EXTRUDE %ls OPEN_CUT OPEN_SK CUT BLIND 5 %ls",name,placement[2][2]>0?L"POS":L"NEG");
     }
     else panel_check(0,"find native opening circle");}
    if(revision_dim(model,L"OPEN_SK",10,PRODIMTYPE_DIAMETER,x)){
        CMD(0,L"实测直径 12 mm",L"REV_OPEN %ls OPEN_CUT OPEN_SK CIRCLE %ls 10 12",name,x);
        VOLUME(96000-36*AS_PI*5);
    }else panel_check(0,"find native opening driver");
    MAKE();
    CMD(0,NULL,L"SKETCH_CREATE %ls RECT_SK SURFACE:%d PLANE:RIGHT RECT 22 23 16 12",name,f.top);
    {FdSketch sk={0};ProMatrix placement;
     error=fd_sketch_open(model,L"RECT_SK",&sk,0);
     if(!error)error=ProSectionLocationGet(sk.section,placement);
     fd_sketch_free(&sk);
     if(!error)CMD(0,NULL,L"SOLID_EXTRUDE %ls RECT_CUT RECT_SK CUT BLIND 6 %ls",name,placement[2][2]>0?L"POS":L"NEG");
     else panel_check(0,"read rectangle sketch direction");}
    if(revision_dim(model,L"RECT_SK",16,PRODIMTYPE_LINEAR,x)&&revision_dim(model,L"RECT_SK",12,PRODIMTYPE_LINEAR,y)){
        CMD(0,L"实测宽高 20 × 14 mm",L"REV_OPEN %ls RECT_CUT RECT_SK RECT %ls 16 20 %ls 12 14",name,x,y);
        VOLUME(96000-20*14*6);
    }else panel_check(0,"find rectangle opening drivers");
    MAKE();
    HOLE(L"BLIND_A",25,25,6,L"BLIND",5);
    HOLE(L"BLIND_B",55,30,6,L"BLIND",5);
    CMD(1,L"恢复",L"REV_DEPTH %ls 5 25 2 BLIND_A BLIND_B",name);
    CMD(0,L"逐孔核对孔底",L"REV_DEPTH %ls 5 8 2 BLIND_A BLIND_B",name);
    VOLUME(96000-2*9*AS_PI*8);
    MAKE();
    CMD(0,NULL,L"HOLE_COUNTERBORE %ls BORE_A SURFACE:%d SURFACE:%d 22 SURFACE:%d 20 6 12 4 THROUGH 0",name,f.top,f.x,f.y);
    CMD(0,NULL,L"HOLE_COUNTERBORE %ls BORE_B SURFACE:%d SURFACE:%d 52 SURFACE:%d 35 6 12 4 THROUGH 0",name,f.top,f.x,f.y);
    CMD(0,L"柱形沉孔",L"REV_RECESS %ls BORE 12 14 4 5 2 BORE_A BORE_B",name);
    VOLUME(96000-2*(9*20+40*5)*AS_PI);
    MAKE();
    CMD(0,NULL,L"HOLE_COUNTERSINK %ls SINK_A SURFACE:%d SURFACE:%d 35 SURFACE:%d 30 6 12 90 THROUGH 0",name,f.top,f.x,f.y);
    CMD(0,L"锥形沉孔",L"REV_RECESS %ls SINK 12 14 90 100 1 SINK_A",name);
    VOLUME(96000-(9*20+(49+21-18)*4/(3*tan(100*AS_PI/360)))*AS_PI);
    MAKE();
    CMD(0,NULL,L"EDGE_ROUND %ls ROUND_A 1 EDGE:%d",name,f.vertical[0]);
    CMD(0,NULL,L"EDGE_ROUND %ls ROUND_B 2 EDGE:%d",name,f.vertical[2]);
    CMD(0,L"统一为 R3 mm",L"REV_ROUND %ls 3 2 ROUND_A 1 ROUND_B 2",name);
    VOLUME(96000-2*9*20*(1-AS_PI/4));
    {ModelPlan plate={0,80,60,2,0,1};
     const char *stage="revision sheet fixture";
     error=model_create(&plate,name,&stage);
     if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
     if(!error)error=panel_activate(model);
     PanelFaceFixture top={model,2,0};
     if(!error)error=ProSolidSurfaceVisit((ProSolid)model,panel_pick_top,NULL,&top);
     if(!error&&top.id)error=CMD(0,NULL,L"SMT_CONVERT %ls TO_SMT SURFACE:%d 2",name,top.id);
     panel_check(!error&&top.id,"create original sheet metal fixture");
     if(!error&&top.id){
         CMD(1,NULL,L"REV_THICK %ls 1 3",name);
         CMD(0,L"改为 3 mm",L"REV_THICK %ls 2 3",name);
         VOLUME(80*60*3);
     }}
    MAKE();
    CMD(0,L"改型副本",L"REV_COPY %ls RAW_COPY",name);
    CMD(1,NULL,L"REV_COPY %ls RAW_COPY",name);
    if(revision_dim(model,NULL,80,PRODIMTYPE_LINEAR,x)){
        CMD(1,L"移除未完成的副本",L"REV_COPY %ls FAIL_COPY 80 60 20 88 60 20 1 %ls 80 87",name,x);
        CMD(0,L"按目标外形改好",L"REV_COPY %ls SIZED_COPY 80 60 20 88 60 20 1 %ls 80 88",name,x);
        ProMdl copy=NULL,failed=NULL;
        panel_check(!ProMdlnameInit(L"SIZED_COPY",PRO_MDL_PART,&copy)&&fabs(volume(copy)-88*60*20)<0.03,
            "revised copy geometry matches target");
        panel_check(ProMdlnameInit(L"FAIL_COPY",PRO_MDL_PART,&failed)!=PRO_TK_NO_ERROR,"failed copy removed from session");
    }else panel_check(0,"find original dimension for copy");
    VOLUME(96000);
#undef VOLUME
#undef MAKE
#undef HOLE
#undef CMD
done:
    trace("REV_CASES",cases);
    trace("REV_FAILURES",failures);
    trace("REV_DONE",1);
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
    ProError error=ProCmdActionAdd("BWTLRevisionProbe",revision_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLRevisionProbe`;");
}

__declspec(dllexport) void user_terminate(void){foundation_reset();}
