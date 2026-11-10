#define BWTL_CABINET_FAULT_TEST
#define user_initialize sheetmetal_fixture_initialize
#define user_terminate sheetmetal_fixture_terminate
#include "native_sheetmetal.c"
#undef user_initialize
#undef user_terminate
#undef EMPTY
#undef PLATE
#undef CASE

static void cabinet_check(int pass,const char *label){
    cases++;
    if(!pass)failures++;
    trace(label,pass?0:-1);
}

static ProError cabinet_command(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
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
    cabinet_check(pass,"cabinet command and feature transaction");
    FILE *log=_wfopen(L"cabinet-cases.log",L"a, ccs=UTF-8");
    if(log){fwprintf(log,L"%ls\nRESULT=%d PASS=%d\n%ls\n",protocol,error,pass,report);fclose(log);}
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
    return error;
}

static ProError cabinet_empty(ProMdl *model,ProMdlName name){
    const char *stage="empty cabinet";
    int window;
    ProError error=model_create_empty(2,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    if(!error)error=ProMdlWindowGet(*model,&window);
    if(!error)error=ProWindowCurrentSet(window);
    return error;
}

typedef struct {ProMdl model;ProSurface face;int id;} CabinetCircleSearch;
static ProError cabinet_circle_edge(ProEdge edge,ProError status,ProAppData data){
    if(status)return status;
    CabinetCircleSearch *search=data;
    ProEnttype type;
    ProVector a,b,d1,d2,d3;
    if(!ProEdgeTypeGet(edge,&type)&&(type==PRO_ENT_CIRCLE||type==PRO_ENT_ARC)&&
       !ProEdgeXyzdataEval(edge,0,a,d1,d2,d3)&&!ProEdgeXyzdataEval(edge,1,b,d1,d2,d3)){
        ProEdgeIdGet(edge,&search->id);
        trace("test circular edge endpoint separation mm", (int)(cm_distance(a,b)*1000));
    }
    return PRO_TK_NO_ERROR;
}
static ProError cabinet_circle_contour(ProContour contour,ProError status,ProAppData data){
    CabinetCircleSearch *search=data;
    return status?status:ProContourEdgeVisit(search->face,contour,cabinet_circle_edge,NULL,search);
}

static int cabinet_probe(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    wchar_t text[1024];
    ShFixture f;
    CabinetEdge base,tip;
    ProFeature wall;
    ProError error;
    model_background_mode=0;
    if(cabinet_empty(&model,name)){cabinet_check(0,"sheet metal warmup");goto done;}
#define CMD(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),cabinet_command(model,text,reject,expected))
#define EMPTY() do{error=cabinet_empty(&model,name);if(error){trace("empty fixture",error);cabinet_check(0,"empty fixture");goto done;}}while(0)
#define PLATE() do{EMPTY();if(CMD(0,L"创建首个板壁后才有钣金实体",L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name)|| \
    CMD(0,NULL,L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name))goto done;sh_test_refs(model,&f);}while(0)
#define EDGE() do{swprintf_s(text,_countof(text),L"EDGE:%d",f.edge);error=cm_edge_get(model,text,&base);if(error){trace("fixture edge",error);cabinet_check(0,"locate fixture edge");goto done;}}while(0)
    PLATE();
    CMD(1,L"未完成",L"CASE_SEG %ls SEG_BAD EDGE:%d 16 90 2 POS 2 8 25 40 100",name,f.edge);
    CMD(0,L"2 段独立翻边",L"CASE_SEG %ls SEG EDGE:%d 16 90 2 POS 2 8 24 42 64",name,f.edge);
    EMPTY();
    CMD(0,L"尚无实体板壁",L"SMT_INFO %ls",name);
    CMD(1,L"退让至少须 4 mm",L"CASE_TRAY %ls INVALID PLANE:FRONT PLANE:RIGHT 200 150 2 30 2 3 POS",name);
    CMD(0,L"四边翻边托盘",L"CASE_TRAY %ls TRAY PLANE:FRONT PLANE:RIGHT 90 70 2 20 2 8 POS",name);
    CMD(1,L"未完成",L"CASE_U %ls NOT_EMPTY PLANE:FRONT PLANE:RIGHT 90 65 2 20 2 POS",name);
    EMPTY();
    CMD(0,L"内净宽 65 mm",L"CASE_U %ls UCASE PLANE:FRONT PLANE:RIGHT 90 65 2 20 2 POS",name);
    PLATE();
    CMD(0,L"高差 22 mm",L"CASE_Z %ls JOG EDGE:%d 22 14 2 POS",name,f.edge);
    PLATE();
    EDGE();
    if(!CMD(0,NULL,L"SMT_FLANGE %ls WALL EDGE:%d 22 90 2 POS",name,f.edge)){
        if(!cm_feature_find(model,L"WALL",&wall)&&!cm_wall_tip(model,&wall,&base,&tip)){
            CMD(1,L"未完成",L"CASE_END %ls WALL EDGE:%d END 100 1",name,tip.id);
            CMD(0,L"直线缩短 5 mm",L"CASE_END %ls WALL EDGE:%d START 5 1",name,tip.id);
        }else cabinet_check(0,"find existing wall tip");
    }
    PLATE();
    EDGE();
    if(!CMD(0,NULL,L"SMT_FLANGE %ls WALL EDGE:%d 22 90 2 POS",name,f.edge)&&
       !cm_feature_find(model,L"WALL",&wall)&&!cm_wall_tip(model,&wall,&base,&tip))
        CMD(0,L"第二道 90° 回折",L"CASE_RETURN %ls RETURN EDGE:%d SURFACE:%d 10 2 POS",name,tip.id,base.face);
    else cabinet_check(0,"find parent flange for return fold");
    PLATE();
    CMD(0,L"中心固定孔直径 4 mm",L"CASE_EAR %ls EAR EDGE:%d 18 42 18 2 POS 4",name,f.edge);
    CMD(1,L"未完成",L"CASE_EAR %ls BAD_EAR EDGE:%d 1 3 18 2 POS 20",name,f.edge);
    PLATE();
    CMD(0,NULL,L"SMT_SKETCH %ls ROUND_SK PLANE:FRONT PLANE:RIGHT CIRCLE 40 30 12",name);
    CMD(0,NULL,L"SMT_CUT %ls ROUND_HOLE SKETCH:ROUND_SK NEG",name);
    sh_test_refs(model,&f);
    {CabinetCircleSearch circle={model,NULL,0};
     if(!ProSurfaceInit((ProSolid)model,f.face,&circle.face))
         ProSurfaceContourVisit(circle.face,cabinet_circle_contour,NULL,&circle);
     cabinet_check(circle.id>0,"find existing native circular hole edge");
     if(circle.id)CMD(0,L"孔轴仍贯通",L"CASE_COLLAR %ls COLLAR EDGE:%d 7 2 POS",name,circle.id);}
    PLATE();
    EDGE();
    CabinetBlank blank={0};
    blank.model=model;
    blank.face=base.face;
    ProSurface face;
    error=ProSurfaceInit((ProSolid)model,base.face,&face);
    if(!error)error=ProSurfaceContourVisit(face,cm_blank_contour,NULL,&blank);
    cabinet_check(!error&&blank.count==4,"collect four native plate edges");
    if(!error&&blank.count==4){
        int adjacent=-1,opposite=-1;
        for(int i=0;i<blank.count;i++){
            CabinetEdge *edge=&blank.edges[i];
            if(fabs(ad_dot(edge->axis,base.axis))<1e-8&&
               (cm_distance(edge->a,base.a)<0.001||cm_distance(edge->b,base.a)<0.001))adjacent=i;
            if(edge->id!=base.id&&fabs(ad_dot(edge->axis,base.axis))>0.999)opposite=i;
        }
        if(adjacent>=0&&opposite>=0){
            FdTxn fixture={0};
            fixture.model=model;
            fixture.scale=1;
            error=cm_attach(&fixture,L"PRIMARY",&base,8,base.length,20,90,2,L"POS",&wall);
            cabinet_check(!error,"create native primary flange with corner allowance");
            CMD(0,NULL,L"SMT_FLANGE %ls NEIGHBOR EDGE:%d 23 90 2 POS",name,blank.edges[adjacent].id);
            ProFeature neighbor;
            CabinetFace neighbor_face;
            if(!cm_feature_find(model,L"PRIMARY",&wall)&&!cm_wall_tip(model,&wall,&base,&tip)&&
               !cm_feature_find(model,L"NEIGHBOR",&neighbor)&&!cm_wall_face(&neighbor,&neighbor_face))
                CMD(0,L"缝隙已从",L"CASE_GAP %ls PRIMARY EDGE:%d SURFACE:%d 12 1",name,tip.id,neighbor_face.id);
            else cabinet_check(0,"find adjacent-wall gap references");
        }else cabinet_check(0,"locate perpendicular and opposite plate edges");
    }
    PLATE();
    EDGE();
    CabinetEdge opposite;
    ProVector shift;
    cm_point(base.a,base.inward,60,shift);
    CabinetEdgeSearch search={0};
    CabinetEdge target=base;
    memcpy(target.a,shift,sizeof(ProVector));
    cm_point(base.b,base.inward,60,target.b);
    search.model=model;search.face_id=base.face;search.line=&target;search.to=base.length;search.tolerance=0.001;
    error=cm_edges_on_face(&search);
    cabinet_check(!error&&search.matches==1,"find opposite free edge for bend batch");
    if(!error&&search.matches==1){
        opposite=search.edge;
        CMD(0,NULL,L"SMT_FLANGE %ls PRIMARY EDGE:%d 20 90 2 POS",name,base.id);
        CMD(0,NULL,L"SMT_FLANGE %ls OTHER EDGE:%d 18 90 2 POS",name,opposite.id);
        CMD(1,L"未完成",L"CASE_BENDS %ls 2 PRIMARY 90 2 85 3 OTHER 89 2 80 3",name);
        cm_test_fail_bend_after=1;
        CMD(1,L"未完成",L"CASE_BENDS %ls 2 PRIMARY 90 2 85 3 OTHER 90 2 80 3",name);
        cm_test_fail_bend_after=0;
        cabinet_check(cm_test_bend_fault_hit,"injected fault occurs after first bend mutation");
        CMD(0,L"已调整 2 道折弯",L"CASE_BENDS %ls 2 PRIMARY 90 2 85 3 OTHER 90 2 80 3",name);
    }
    PLATE();
    CMD(0,NULL,L"SMT_SKETCH %ls BENDLINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",name);
    CMD(0,NULL,L"SMT_BEND %ls BENDONE CURVE:BENDLINE 90 2 POS FIX1",name);
    CMD(0,L"已调整 1 道折弯",L"CASE_BENDS %ls 1 BENDONE 90 2 80 3",name);
#undef EDGE
#undef PLATE
#undef EMPTY
#undef CMD
done:
    trace("CABINET_CASES",cases);
    trace("CABINET_FAILURES",failures);
    trace("CABINET_DONE",1);
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
    ProError error=ProCmdActionAdd("BWTLCabinetProbe",cabinet_probe,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLCabinetProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
