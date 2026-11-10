#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>

typedef struct {
    ProMdl model;
    int hole_a,hole_b,slot_a,slot_b;
} QualityFixtureFaces;

static ProError qa_test_faces(ProSurface surface,ProError status,ProAppData data){
    QualityFixtureFaces *faces=data;
    ProGeomitemdata *raw=NULL;
    int id;
    if(status)return status;
    if(ProSurfaceIdGet(surface,&id)||ProSurfaceDataGet(surface,&raw))return 0;
    const ProSurfacedata *geometry=raw->data.p_surface_data;
    if(geometry->type==PRO_SRF_CYL){
        double x=geometry->srf_shape.cylinder.origin[0],y=geometry->srf_shape.cylinder.origin[1];
        if(part_close(y,20)&&part_close(x,20))faces->hole_a=id;
        if(part_close(y,20)&&part_close(x,45))faces->hole_b=id;
    }else if(geometry->type==PRO_SRF_PLANE){
        ProVector point,normal;
        ProUvParam uv={0,0};
        if(!ProSurfaceXyzdataEval(surface,uv,point,NULL,NULL,normal)&&fabs(normal[0])>0.999){
            if(part_close(point[0],30))faces->slot_a=id;
            if(part_close(point[0],50))faces->slot_b=id;
        }
    }
    ProGeomitemdataFree(&raw);
    return 0;
}

typedef struct {int outer,tangent;} QualityFixtureEdges;
static ProError qa_test_edges(ProGeomitem *item,ProError status,ProAppData data){
    QualityFixtureEdges *found=data;
    ProEdge edge;
    ProSurface a,b;
    ProSrftype first,second;
    ProVector p,q;
    if(status)return status;
    if(ProGeomitemToEdge(item,&edge))return 0;
    if(!ProEdgeXyzdataEval(edge,0,p,NULL,NULL,NULL)&&!ProEdgeXyzdataEval(edge,1,q,NULL,NULL,NULL)){
        if(part_close(p[0],0)&&part_close(q[0],0)&&part_close(p[2],20)&&part_close(q[2],20)&&
           part_close(fabs(p[1]-q[1]),60))found->outer=item->id;
    }
    if(!ProEdgeNeighborsGet(edge,NULL,NULL,&a,&b)&&a&&b&&
       !ProSurfaceTypeGet(a,&first)&&!ProSurfaceTypeGet(b,&second)&&
       ((first==PRO_SRF_CYL&&second==PRO_SRF_PLANE)||(first==PRO_SRF_PLANE&&second==PRO_SRF_CYL))){
        ProEnttype type;
        if(!ProEdgeTypeGet(edge,&type)&&type==PRO_ENT_LINE)found->tangent=item->id;
    }
    return 0;
}

static ProError qa_test_feature_edges(ProFeature *feature,ProError status,ProAppData data){
    if(status)return status;
    ProError error=ProFeatureGeomitemVisit(feature,PRO_EDGE,qa_test_edges,NULL,data);
    return error==PRO_TK_E_NOT_FOUND?0:error;
}

static void qa_test(ProMdl model,const wchar_t *protocol,int reject,const wchar_t *expected){
    PartCommand command;
    PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));
    ProWVerstamp after=NULL;
    ProMdl active=NULL;
    wchar_t message[PART_TEXT_MAX]=L"";
    double old_volume=volume(model);
    ProError error=snapshot?part_snapshot_take_mode(model,snapshot,0):PRO_TK_OUT_OF_MEMORY;
    if(!error){
        if(part_command_parse(protocol,&command)!=1)error=PRO_TK_BAD_INPUTS;
        else error=part_execute(&command,snapshot,message,_countof(message));
    }
    int unchanged=snapshot&&snapshot->stamp&&
        !ProMdlVerstampGet(model,&after)&&ProVerstampEqual(after,snapshot->stamp)&&
        !ProMdlCurrentGet(&active)&&active==model&&part_close(volume(model),old_volume);
    int pass=unchanged&&(reject?error!=0:error==0)&&(!expected||wcsstr(message,expected));
    cases++;
    if(!pass)failures++;
    FILE *log=_wfopen(L"quality-cases.log",L"a, ccs=UTF-8");
    if(log){
        fwprintf(log,L"%ls\nRESULT=%d UNCHANGED=%d PASS=%d EXPECTED=%ls\n%ls\n",
            protocol,error,unchanged,pass,expected?expected:L"",message);
        fclose(log);
    }
    if(after)ProVerstampFree(&after);
    if(snapshot){part_snapshot_clear(snapshot);free(snapshot);}
}

static int qa_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL,first=NULL;
    ProMdlName name,first_name;
    Fixture f;
    QualityFixtureEdges edges={0};
    QualityFixtureFaces faces={0};
    wchar_t text[1200];
    model_background_mode=0;
#define NEW() do{int window;ProError e=fixture(&model,name,&f);if(!e)e=ProMdlWindowGet(model,&window);if(!e)e=ProWindowCurrentSet(window);if(e){trace("quality fixture",e);failures++;goto done;}}while(0)
#define READ(reject,expected,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),qa_test(model,text,reject,expected))
#define BUILD(format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),run(model,text,0))
#define HOLE(label,px,py) BUILD(L"HOLE %ls %ls SURFACE:%d SURFACE:%d %d SURFACE:%d %d 8 THROUGH 0",name,label,f.top,f.x,px,f.y,py)
    /* Establish a part window before testing production current-model binding. */
    NEW();
    NEW();
    ProSolidFeatVisit((ProSolid)model,qa_test_feature_edges,NULL,&edges);
    READ(0,L"= 20 mm",L"QA_MEASURE %ls LENGTH EDGE:%d",name,f.vertical[0]);
    READ(0,L"= 4800 mm²",L"QA_MEASURE %ls AREA SURFACE:%d",name,f.top);
    READ(0,L"= 20 mm",L"QA_MEASURE %ls DISTANCE SURFACE:%d SURFACE:%d",name,f.top,f.bottom);
    READ(0,L"= 90 °",L"QA_MEASURE %ls ANGLE EDGE:%d EDGE:%d",name,f.vertical[0],edges.outer);
    READ(0,L"最小壁厚 20 mm",L"QA_WALL %ls 19 SURFACE:%d SURFACE:%d",name,f.top,f.bottom);
    READ(0,L"低于要求",L"QA_WALL %ls 21 SURFACE:%d SURFACE:%d",name,f.bottom,f.top);
    READ(0,L"未判定面组 1",L"QA_WALL %ls 1 SURFACE:%d SURFACE:%d",name,f.top,f.top);
    READ(0,L"90° 至 90°",L"QA_DRAFT %ls SURFACE:%d Z POS 10",name,f.top);
    READ(0,L"-90° 至 -90°",L"QA_DRAFT %ls SURFACE:%d Z NEG 10",name,f.top);
    READ(0,L"G0，切向超限",L"QA_CONTINUITY %ls EDGE:%d 0.1 0.01",name,f.vertical[0]);
    READ(0,L"实体体积 96000 mm³，表面积 15200 mm²",L"QA_MASS %ls",name);
    BUILD(L"DATUM_PLANE %ls MID_X PLANE:RIGHT 40",name);
    READ(0,L"采样范围内未发现超出容差的差异",L"QA_SYMMETRY %ls PLANE:MID_X 0.01 9",name);
    BUILD(L"REUSE_PARAM %ls PART_CODE STRING QC",name);
    READ(0,L"符合 3，不符合 2，未判定 1",L"QA_RULES %ls MIN_VOLUME 90000 MAX_VOLUME 95000 REQUIRE_PARAM PART_CODE REQUIRE_PARAM MISSING_CODE MIN_WALL SURFACE:%d SURFACE:%d 20 MIN_WALL SURFACE:%d SURFACE:%d 1",name,f.top,f.bottom,f.top,f.top);
    HOLE(L"BORE_A",20,20);
    READ(0,L"检测到几何差异",L"QA_SYMMETRY %ls PLANE:MID_X 0.01 15",name);
    ProMdl identical;
    if(ProMdlnameCopy(model,L"QA_IDENTICAL",&identical)){failures++;goto done;}
    READ(0,L"采样范围内未发现超出容差的差异",L"QA_COMPARE %ls QA_IDENTICAL 0.01 9",name);
    ProModelitem top;
    ProFeature base;
    if(ProModelitemInit(model,f.top,PRO_SURFACE,&top)||ProGeomitemFeatureGet(&top,&base)){failures++;goto done;}
    READ(0,L"BORE_A",L"QA_DEPS %ls ID:%d DOWN 2",name,base.id);
    READ(0,L"上游参照",L"QA_DEPS %ls BORE_A BOTH 3",name);
    first=model;
    wcscpy_s(first_name,_countof(first_name),name);

    NEW();
    HOLE(L"SHIFTED_BORE",45,20);
    READ(0,L"检测到几何差异",L"QA_COMPARE %ls %ls 0.01 17",name,first_name);
    READ(0,L"净变化 +0 mm³",L"QA_COMPARE %ls %ls 0.01 17",name,first_name);
    READ(1,L"未完成",L"QA_COMPARE %ls MISSING_QA_PART 0.01 7",name);
    READ(1,L"未完成",L"QA_COMPARE %ls %ls 0.01 7",name,name);
    if(!part_close(volume(model),volume(first))){trace("version equal-volume fixture",-1);failures++;}
    HOLE(L"BORE_A",20,20);
    faces.model=model;
    ProSolidSurfaceVisit((ProSolid)model,qa_test_faces,NULL,&faces);
    READ(0,L"= 8 mm",L"QA_MEASURE %ls DIAMETER SURFACE:%d",name,faces.hole_a);
    READ(0,L"中心距 25 mm，投影净距 17 mm",L"QA_HOLES %ls PAIR SURFACE:%d SURFACE:%d 18",name,faces.hole_a,faces.hole_b);
    edges.outer=0;
    ProSolidFeatVisit((ProSolid)model,qa_test_feature_edges,NULL,&edges);
    READ(0,L"最短距离 20 mm，投影净距 16 mm",L"QA_HOLES %ls EDGE SURFACE:%d EDGE:%d 15",name,faces.hole_a,edges.outer);
    READ(0,L"0° 至 0°",L"QA_DRAFT %ls SURFACE:%d Z POS 1",name,faces.hole_a);
    READ(1,L"未完成",L"QA_HOLES %ls PAIR SURFACE:%d SURFACE:%d 1",name,faces.hole_a,faces.hole_a);
    READ(0,L"符合 2，不符合 1，未判定 0",L"QA_RULES %ls MIN_HOLE_GAP SURFACE:%d SURFACE:%d 18 MIN_EDGE_GAP SURFACE:%d EDGE:%d 15 MIN_DRAFT SURFACE:%d Z 80",name,faces.hole_a,faces.hole_b,faces.hole_a,edges.outer,f.top);

    NEW();
    BUILD(L"DATUM_PLANE %ls SLOT_Z PLANE:FRONT 20",name);
    BUILD(L"SKETCH_CREATE %ls SLOT_SK PLANE:SLOT_Z PLANE:RIGHT RECT 30 22 20 16",name);
    BUILD(L"SOLID_EXTRUDE %ls SLOT SLOT_SK CUT THROUGH 0 POS",name);
    memset(&faces,0,sizeof(faces));
    faces.model=model;
    ProSolidSurfaceVisit((ProSolid)model,qa_test_faces,NULL,&faces);
    READ(0,L"未判定面组 1",L"QA_WALL %ls 1 SURFACE:%d SURFACE:%d",name,faces.slot_a,faces.slot_b);

    NEW();
    BUILD(L"EDGE_ROUND %ls TEST_ROUND 2 EDGE:%d",name,f.vertical[0]);
    memset(&edges,0,sizeof(edges));
    ProSolidFeatVisit((ProSolid)model,qa_test_feature_edges,NULL,&edges);
    READ(0,L"G0 与 G1 符合",L"QA_CONTINUITY %ls EDGE:%d 0.1 0.01",name,edges.tangent);

done:
    trace("QUALITY_CASES",cases);
    trace("QUALITY_FAILURES",failures);
    trace("QUALITY_DONE",1);
    ProEngineerEnd();
    return 0;
#undef NEW
#undef READ
#undef BUILD
#undef HOLE
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
    ProError error=ProCmdActionAdd("BWTLQualityProbe",qa_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLQualityProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
