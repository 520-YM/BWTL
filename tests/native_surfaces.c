#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>

static void sf_test_offset_plane(ProMdl model,const wchar_t *name,double distance){
    FdRef a={0},b={0};SfArea aa,bb;ProGeomitem ai,bi;ProVector p,n,q,m;double dot=0;
    wchar_t token[64];ProError e;
    swprintf_s(token,_countof(token),L"QUILT:%ls",name);
    e=sf_ref(model,L"QUILT:WALL",&a);
    if(!e)e=sf_ref(model,token,&b);
    if(!e)e=sf_area(&a,&aa);
    if(!e)e=sf_area(&b,&bb);
    if(!e)e=ProSurfaceToGeomitem((ProSolid)model,aa.single,&ai);
    if(!e)e=ProSurfaceToGeomitem((ProSolid)model,bb.single,&bi);
    if(!e)e=fd_plane_data(&ai,p,n);
    if(!e)e=fd_plane_data(&bi,q,m);
    if(!e){for(int i=0;i<3;i++)dot+=(q[i]-p[i])*n[i];if(fabs(dot-distance)>1e-6)e=PRO_TK_GENERAL_ERROR;}
    trace("verify signed offset distance",e);cases++;if(e)failures++;
    fd_ref_free(&a);fd_ref_free(&b);
}
static int sf_test_quilt(ProMdl model,const wchar_t *name,double expected){
    wchar_t token[64];
    FdRef ref={0};
    SfArea area;
    ProError e;
    swprintf_s(token,_countof(token),L"QUILT:%ls",name);
    e=sf_ref(model,token,&ref);
    if(!e)e=sf_area(&ref,&area);
    if(!e&&fabs(area.area-expected)>fmax(0.05,expected*0.0005))e=PRO_TK_GENERAL_ERROR;
    trace("verify exact quilt area",e);cases++;if(e)failures++;
    fd_ref_free(&ref);
    return !e;
}

static int sf_test_bodies(ProMdl model,int ids[2]){
    ProSolidBody *bodies=NULL;
    int n,count=0;
    ProError e=ProSolidBodiesCollect((ProSolid)model,&bodies);
    if(!e)e=ProArraySizeGet((ProArray)bodies,&n);
    if(!e)for(int i=0;i<n;i++){
        ProSolidBodyState state;
        if((e=ProSolidBodyStateGet(&bodies[i],&state)))break;
        if(state==PRO_BODY_STATE_ACTIVE&&count<2)ids[count++]=bodies[i].id;
    }
    if(bodies)ProArrayFree((ProArray *)&bodies);
    return !e&&count==2;
}
static ProError sf_test_edge(ProEdge edge,ProError status,ProAppData data){
    int *id=data;
    ProVector start,end;
    if(status)return status;
    if(ProEdgeXyzdataEval(edge,0,start,NULL,NULL,NULL)||ProEdgeXyzdataEval(edge,1,end,NULL,NULL,NULL))return PRO_TK_GENERAL_ERROR;
    if(fabs(start[1])<1e-6&&fabs(end[1])<1e-6)return ProEdgeIdGet(edge,id);
    return PRO_TK_NO_ERROR;
}
typedef struct {ProSurface surface;int id;} SfTestEdge;
static ProError sf_test_contour(ProContour contour,ProError status,ProAppData data){
    SfTestEdge *found=data;
    if(status)return status;
    return ProContourEdgeVisit(found->surface,contour,sf_test_edge,NULL,&found->id);
}
static int sf_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    const char *stage="new surface fixture";
    ProError e;
    wchar_t text[1600];
    double pi=3.141592653589793;
    int body_ids[2];
    model_background_mode=1;
#define EMPTY() do{if((e=model_create_empty(1,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}}while(0)
#define CASE(reject,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),run(model,text,reject))
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls WALL_SK PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",name);
    CASE(0,L"SURFACE_EXTRUDE %ls WALL WALL_SK 10 POS",name);
    sf_test_quilt(model,L"WALL",200);
    CASE(0,L"SURFACE_INFO %ls",name);
    CASE(0,L"SURFACE_OFFSET %ls OFFSET_A QUILT:WALL 3",name);
    sf_test_quilt(model,L"OFFSET_A",200);
    sf_test_offset_plane(model,L"OFFSET_A",3);
    CASE(0,L"SURFACE_OFFSET %ls OFFSET_B QUILT:WALL -3",name);
    sf_test_offset_plane(model,L"OFFSET_B",-3);
    CASE(1,L"SURFACE_OFFSET %ls STALE QUILT:999999 3",name);
    CASE(1,L"SURFACE_FILL %ls OPEN_PATCH WALL_SK",name);
    CASE(1,L"SURFACE_SOLIDIFY %ls OPEN_SOLID QUILT:WALL",name);
    sf_test_quilt(model,L"WALL",200);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls REV_SK PLANE:FRONT PLANE:RIGHT LINE 10 0 10 20",name);
    CASE(0,L"DATUM_AXIS %ls REV_AXIS PLANES PLANE:RIGHT PLANE:FRONT",name);
    CASE(0,L"SURFACE_REVOLVE %ls REV_SURF REV_SK AXIS:REV_AXIS 360 POS",name);
    sf_test_quilt(model,L"REV_SURF",400*pi);
    CASE(0,L"SURFACE_OFFSET %ls CYL_OFFSET QUILT:REV_SURF 2",name);
    sf_test_quilt(model,L"CYL_OFFSET",480*pi);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name);
    sf_test_quilt(model,L"PATCH",200);
    CASE(0,L"DATUM_PLANE %ls CUT_PLANE PLANE:RIGHT 10",name);
    CASE(0,L"SURFACE_TRIM %ls HALF_PATCH QUILT:PATCH PLANE:CUT_PLANE POS",name);
    sf_test_quilt(model,L"PATCH",100);
    CASE(1,L"SURFACE_TRIM %ls NO_CHANGE QUILT:PATCH PLANE:FRONT POS",name);
    sf_test_quilt(model,L"PATCH",100);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name);
    CASE(0,L"DATUM_PLANE %ls TOOL_PLANE PLANE:FRONT 5",name);
    CASE(0,L"SKETCH_CREATE %ls TOOL_SK PLANE:TOOL_PLANE PLANE:RIGHT LINE 10 -5 10 15",name);
    CASE(0,L"SURFACE_EXTRUDE %ls TOOL_SURF TOOL_SK 20 POS",name);
    CASE(0,L"SURFACE_TRIM %ls QUILT_TRIM QUILT:PATCH QUILT:TOOL_SURF POS",name);
    sf_test_quilt(model,L"PATCH",100);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name);
    CASE(0,L"SKETCH_CREATE %ls CUT_LINE PLANE:FRONT PLANE:RIGHT LINE 10 -5 10 15",name);
    CASE(0,L"SURFACE_TRIM %ls CURVE_TRIM QUILT:PATCH CURVE:CUT_LINE NEG",name);
    sf_test_quilt(model,L"PATCH",100);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    if(!CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name)){
        FdRef ref={0};SfArea area;SfTestEdge edge={0};
        if(sf_ref(model,L"QUILT:PATCH",&ref)||sf_area(&ref,&area)){failures++;}
        else{
            edge.surface=area.single;
            if(ProSurfaceContourVisit(area.single,sf_test_contour,NULL,&edge)||!edge.id)failures++;
            else {CASE(0,L"SURFACE_EXTEND %ls EXTENDED QUILT:PATCH EDGE:%d 5",name,edge.id);sf_test_quilt(model,L"PATCH",300);}
        }
        fd_ref_free(&ref);
    }
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls A PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",name);
    CASE(0,L"SKETCH_CREATE %ls B PLANE:FRONT PLANE:RIGHT ARC 10 10 10 0 180",name);
    CASE(0,L"SURFACE_BOUNDARY %ls BLENDED CURVE:A CURVE:B",name);
    sf_test_quilt(model,L"BLENDED",357.066198);
    CASE(1,L"SURFACE_BOUNDARY %ls BLENDED CURVE:A CURVE:B",name);
    CASE(1,L"SURFACE_BOUNDARY %ls BAD_BOUND CURVE:A CURVE:MISSING",name);
    sf_test_quilt(model,L"BLENDED",357.066198);
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    if(!CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name)){
        CASE(0,L"SURFACE_THICKEN %ls PLATE QUILT:PATCH 2 SYM",name);
        expect_volume(model,400);
    }
    for(int side=0;side<2;side++){
        EMPTY();
        CASE(0,L"SKETCH_CREATE %ls FILL_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
        CASE(0,L"SURFACE_FILL %ls PATCH FILL_SK",name);
        CASE(0,L"SURFACE_THICKEN %ls PLATE QUILT:PATCH 2 %ls",name,side?L"NEG":L"POS");
        expect_volume(model,400);
    }
    EMPTY();
    CASE(0,L"SKETCH_CREATE %ls SIDES_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 20 10",name);
    CASE(0,L"SURFACE_EXTRUDE %ls SIDES SIDES_SK 10 POS",name);
    CASE(0,L"SURFACE_FILL %ls CAP_TOP SIDES_SK",name);
    CASE(0,L"DATUM_PLANE %ls BOTTOM_PLANE PLANE:FRONT -10",name);
    CASE(0,L"SKETCH_CREATE %ls BOTTOM_SK PLANE:BOTTOM_PLANE PLANE:RIGHT RECT 0 0 20 10",name);
    CASE(0,L"SURFACE_FILL %ls CAP_BOTTOM BOTTOM_SK",name);
    CASE(0,L"SURFACE_MERGE %ls CLOSED QUILT:SIDES QUILT:CAP_TOP QUILT:CAP_BOTTOM",name);
    CASE(0,L"SURFACE_SOLIDIFY %ls FILLED QUILT:SIDES",name);
    expect_volume(model,2000);
    for(int mode=0;mode<3;mode++){
        const wchar_t *operation=mode==0?L"MERGE":mode==1?L"SUBTRACT":L"INTERSECT";
        EMPTY();
        CASE(0,L"SKETCH_CREATE %ls BODY_A_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 10 12",name);
        CASE(0,L"BODY_EXTRUDE %ls BODY_A BODY_A_SK 10 POS",name);
        CASE(0,L"SKETCH_CREATE %ls BODY_B_SK PLANE:FRONT PLANE:RIGHT RECT 5 0 10 12",name);
        CASE(0,L"BODY_EXTRUDE %ls BODY_B BODY_B_SK 10 POS",name);
        if(!sf_test_bodies(model,body_ids)){trace("two active bodies", -1);failures++;continue;}
        CASE(1,L"BODY_BOOLEAN %ls BAD_BOOL MERGE BODY:%d BODY:%d",name,body_ids[0],body_ids[0]);
        CASE(0,L"BODY_BOOLEAN %ls BOOL_RESULT %ls BODY:%d BODY:%d",name,operation,body_ids[0],body_ids[1]);
        expect_volume(model,mode==0?1800:600);
    }
done:
    trace("SURFACE_NATIVE_CASES",cases);trace("SURFACE_NATIVE_FAILURES",failures);trace("SURFACE_NATIVE_DONE",1);
    ProEngineerEnd();return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;
    char marker[16];
    ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLSurfaceProbe",sf_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLSurfaceProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
