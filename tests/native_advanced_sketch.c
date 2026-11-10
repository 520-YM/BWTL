#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProContour.h>

typedef struct {ProSurface surface;int ids[4],count;} EdgeLoop;
static ProError fixture_edge(ProEdge edge,ProError status,ProAppData data){
    EdgeLoop *loop=data;
    if(status)return status;
    if(loop->count>=4)return PRO_TK_BAD_INPUTS;
    return ProEdgeIdGet(edge,&loop->ids[loop->count++]);
}
static ProError fixture_loop(ProContour contour,ProError status,ProAppData data){
    EdgeLoop *loop=data;
    if(status)return status;
    return ProContourEdgeVisit(loop->surface,contour,fixture_edge,NULL,data);
}

static void ellipse_check(ProMdl model){
    FdSketch s={0};Pro2dEntdef *entity=NULL;
    AsPoint a=as_ray(31*AS_PI/180),b=as_left(a),center=as_point(12,14),p;
    ProError e=fd_sketch_open(model,L"ELL",&s,0);
    if(!e)e=ProSectionEntityGet(s.section,0,&entity);
    if(!e&&entity->type!=PRO_2D_B_SPLINE)e=PRO_TK_INVALID_TYPE;
    if(!e)for(int i=0;i<=32;i++){
        double u,v;
        if((e=as_point_at(s.section,0,entity,i/32.0,&p)))break;
        p=as_minus(p,center);u=as_dot2(p,a)/12;v=as_dot2(p,b)/4;
        if(fabs(u*u+v*v-1)>1e-6){e=PRO_TK_GENERAL_ERROR;break;}
    }
    trace("committed rotated ellipse follows exact conic",e);cases++;if(e)failures++;
    fd_sketch_free(&s);
}

static void shape_check(ProMdl model,const wchar_t *name,int count,ProSectionShape want){
    FdSketch s={0};
    ProSectionShape shape;
    int *ids=NULL,n;
    ProError e;
    if((e=fd_sketch_open(model,name,&s,0))||(e=ProSectionEntityIdsGet(s.section,&ids,&n))||
       (e=ProSectionShapeGet(s.section,&shape)))goto done;
    if(n!=count||shape!=want){trace("unexpected entity count",n);trace("unexpected sketch shape",shape);e=PRO_TK_GENERAL_ERROR;}
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&s);trace("native sketch shape and count",e);cases++;if(e)failures++;
}

static void audit_check(ProMdl model,const wchar_t *name,int gaps,int duplicates,int crosses){
    FdSketch s={0};
    AsAudit audit;
    ProError e=fd_sketch_open(model,name,&s,0);
    if(!e)e=as_audit(s.section,1,&audit,NULL);
    if(!e&&(audit.gaps!=gaps||audit.duplicates!=duplicates||audit.crossings!=crosses)){
        trace("audit gap count",audit.gaps);trace("audit repeat count",audit.duplicates);trace("audit intersection count",audit.crossings);
        e=PRO_TK_GENERAL_ERROR;
    }
    fd_sketch_free(&s);trace("native audit against known profile",e);cases++;if(e)failures++;
}

static ProError fixture_lines(ProMdl model,const wchar_t *name,const double lines[][4],int n,int *created){
    FdSketch s={0};
    int i,*ids=NULL,count=0;
    ProErrorlist errors={0};
    ProError e=fd_sketch_open(model,name,&s,0);
    if(e)goto done;
    if((e=ProSectionIntentManagerModeSet(s.section,PRO_B_FALSE))||
       (e=ProSectionEntityIdsGet(s.section,&ids,&count)))goto done;
    for(i=0;i<n;i++)if((e=as_add_line(s.section,as_p(lines[i]),as_p(lines[i]+2),&created[i])))goto done;
    for(i=0;i<count;i++)if((e=ProSectionEntityDelete(s.section,ids[i])))goto done;
    if((e=fd_section_solve(s.section,1))||(e=sd_mark_section(&s)))goto done;
    s.options[0]=PRO_FEAT_CR_NO_OPTS;
    e=ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);
    if(!e)e=part_regenerate(model);
done:
    if(ids)ProArrayFree((ProArray *)&ids);
    fd_sketch_free(&s);trace("build raw temporary sketch fixture",e);if(e)failures++;
    return e;
}

static void new_extent(ProMdl model,const wchar_t *name,double want_x,double want_y){
    FdSketch s={0};AsSnapshot snapshot;
    int i,k;
    double x=-1e10,y=-1e10;
    ProError e=fd_sketch_open(model,name,&s,0);
    if(!e)e=as_snapshot(s.section,&snapshot);
    if(!e){
        for(i=0;i<snapshot.count;i++)if(snapshot.items[i].sampled&&snapshot.items[i].type!=PRO_2D_CENTER_LINE)for(k=0;k<5;k++){
            x=fmax(x,snapshot.items[i].points[k].x);y=fmax(y,snapshot.items[i].points[k].y);
        }
        if(fabs(x-want_x)>0.001||fabs(y-want_y)>0.001)e=PRO_TK_GENERAL_ERROR;
    }
    trace("check numerical committed sketch extent",e);cases++;if(e)failures++;
    fd_sketch_free(&s);
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProMdl model=NULL;ProMdlName name;Fixture block;wchar_t command[1600];char marker[16];
    const double trim_lines[][4]={{0,0,20,0},{12,-5,12,5},{-10,10,-4,10}};
    const double corner_lines[][4]={{0,0,25,0},{0,0,0,20},{-10,5,-4,5}};
    const double gap_lines[][4]={{0,0,20,0},{20,0,20,15},{20,15,0,15},{0,15,0,0.05}};
    const double crossing_lines[][4]={{0,0,10,10},{0,10,10,0},{20,0,28,0},{22,0,26,0}};
    int ids[4]={0};
    EdgeLoop top={0};
    HMODULE modules[1024];DWORD bytes;
    ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(DWORD i=0;i<bytes/sizeof(HMODULE);i++){
        wchar_t path[MAX_PATH];
        if(!GetModuleFileNameW(modules[i],path,_countof(path)))return PRO_TK_GENERAL_ERROR;
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident"))return PRO_TK_BAD_CONTEXT;
    }
    trace("legacy_bridge_loaded",0);
    model_background_mode=1;
    if((e=fixture(&model,name,&block))){trace("create disposable sketch fixture",e);failures++;goto done;}
#define CASE(reject,format,...) do{swprintf_s(command,_countof(command),L"CREO_OP " format,__VA_ARGS__);run(model,command,reject);}while(0)
    CASE(0,L"SKETCH_CREATE %ls ELL PLANE:FRONT PLANE:RIGHT ELLIPSE 12 14 24 8 31",name);
    CASE(0,L"SKETCH_CREATE %ls SLOT_A PLANE:FRONT PLANE:RIGHT SLOT 20 24 30 8 27",name);
    CASE(0,L"SKETCH_CREATE %ls POLY PLANE:FRONT PLANE:RIGHT POLYGON 6 25 20 VERTEX 10 11",name);
    CASE(0,L"SKETCH_CREATE %ls POLY_SIDE PLANE:FRONT PLANE:RIGHT POLYGON 12 20 25 SIDE 10 -9",name);
    CASE(0,L"SKETCH_CREATE %ls SPL_THROUGH PLANE:FRONT PLANE:RIGHT SPLINE THROUGH 0 0 8 3 16 -2 25 5",name);
    CASE(0,L"SKETCH_CREATE %ls SPL_CONTROL PLANE:FRONT PLANE:RIGHT SPLINE CONTROL 0 0 8 3 16 -2 25 5",name);
    CASE(1,L"SKETCH_CREATE %ls BAD_SPL PLANE:FRONT PLANE:RIGHT SPLINE THROUGH 0 0 8 3 8 3",name);
    shape_check(model,L"ELL",1,PRO_SECSHAPE_1_CLOSED_LOOP);
    ellipse_check(model);
    shape_check(model,L"SLOT_A",4,PRO_SECSHAPE_1_CLOSED_LOOP);
    shape_check(model,L"POLY",6,PRO_SECSHAPE_1_CLOSED_LOOP);
    shape_check(model,L"POLY_SIDE",12,PRO_SECSHAPE_1_CLOSED_LOOP);
    shape_check(model,L"SPL_THROUGH",1,PRO_SECSHAPE_1_OPEN_LOOP);
    shape_check(model,L"SPL_CONTROL",1,PRO_SECSHAPE_1_OPEN_LOOP);
    CASE(0,L"SKETCH_CREATE %ls OFFSET_SK PLANE:FRONT PLANE:RIGHT POLYGON 4 0 0 SIDE 10 0",name);
    CASE(0,L"SKETCH_OFFSET %ls OFFSET_SK 2 0 1 2 3",name);
    new_extent(model,L"OFFSET_SK",12,12);
    CASE(1,L"SKETCH_OFFSET %ls OFFSET_SK -100 0 1 2 3",name);
    CASE(0,L"SKETCH_CREATE %ls OFFSET_CIRCLE PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 7",name);
    CASE(0,L"SKETCH_OFFSET %ls OFFSET_CIRCLE 3 0",name);
    new_extent(model,L"OFFSET_CIRCLE",10,10);
    CASE(0,L"SKETCH_CREATE %ls OFFSET_SLOT PLANE:FRONT PLANE:RIGHT SLOT 0 0 30 8 0",name);
    CASE(0,L"SKETCH_OFFSET %ls OFFSET_SLOT 2 0 1 2 3",name);
    new_extent(model,L"OFFSET_SLOT",17,6);
    CASE(1,L"SKETCH_OFFSET %ls OFFSET_SLOT -5 0 1 2 3",name);
    CASE(0,L"SKETCH_CREATE %ls ADD_ADV PLANE:FRONT PLANE:RIGHT CIRCLE -30 -20 3",name);
    CASE(0,L"SKETCH_ADD %ls ADD_ADV ELLIPSE 18 10 28 11 -36",name);
    shape_check(model,L"ADD_ADV",2,PRO_SECSHAPE_MULTI_CLOSED_LOOPS);
    CASE(0,L"SKETCH_CREATE %ls MIRROR_SK PLANE:FRONT PLANE:RIGHT SLOT -15 0 14 6 0",name);
    CASE(0,L"SKETCH_ADD %ls MIRROR_SK CENTERLINE 0 -40 0 40",name);
    CASE(0,L"SKETCH_MIRROR %ls MIRROR_SK 4 0 1 2 3",name);
    new_extent(model,L"MIRROR_SK",22,3);
#define RAW(name_literal,data) do{CASE(0,L"SKETCH_CREATE %ls " name_literal L" PLANE:FRONT PLANE:RIGHT POLYGON 3 35 35 VERTEX 5 0",name);if(fixture_lines(model,name_literal,data,_countof(data),ids))goto done;}while(0)
    RAW(L"TRIM_SK",trim_lines);
    CASE(0,L"SKETCH_TRIM_EXTEND %ls TRIM_SK TRIM END %d %d",name,ids[0],ids[1]);
    new_extent(model,L"TRIM_SK",12,10);
    CASE(0,L"SKETCH_CREATE %ls EXTEND_SK PLANE:FRONT PLANE:RIGHT POLYGON 3 35 35 VERTEX 5 0",name);
    {const double lines[][4]={{0,0,10,0},{14,-5,14,5}};if(fixture_lines(model,L"EXTEND_SK",lines,2,ids))goto done;}
    CASE(0,L"SKETCH_TRIM_EXTEND %ls EXTEND_SK EXTEND END %d %d",name,ids[0],ids[1]);
    CASE(0,L"SKETCH_CREATE %ls FIXED_LEN PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",name);
    CASE(0,L"SKETCH_ADD %ls FIXED_LEN LINE 12 -5 12 5",name);
    CASE(1,L"SKETCH_TRIM_EXTEND %ls FIXED_LEN TRIM END 0 1",name);
    RAW(L"ROUND_SK",corner_lines);
    CASE(0,L"SKETCH_CORNER %ls ROUND_SK ROUND 3 %d %d",name,ids[0],ids[1]);
    RAW(L"CHAMFER_SK",corner_lines);
    CASE(0,L"SKETCH_CORNER %ls CHAMFER_SK CHAMFER 2 %d %d",name,ids[0],ids[1]);
    CASE(0,L"SKETCH_CREATE %ls CHAIN_SK PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 2",name);
    CASE(0,L"SKETCH_PROJECT_CHAIN %ls CHAIN_SK CURVE:POLY",name);
    if((e=ProSurfaceInit((ProSolid)model,block.top,&top.surface))||
       (e=ProSurfaceContourVisit(top.surface,fixture_loop,NULL,&top))||top.count!=4){trace("collect bounded top edges",e);failures++;goto done;}
    CASE(0,L"SKETCH_CREATE %ls EDGE_CHAIN_SK PLANE:FRONT PLANE:RIGHT CIRCLE 5 5 2",name);
    CASE(0,L"SKETCH_PROJECT_CHAIN %ls EDGE_CHAIN_SK EDGE:%d EDGE:%d EDGE:%d EDGE:%d",name,top.ids[0],top.ids[1],top.ids[2],top.ids[3]);
    CASE(1,L"SKETCH_PROJECT_CHAIN %ls EDGE_CHAIN_SK EDGE:%d EDGE:%d",name,top.ids[0],top.ids[2]);
    RAW(L"GAP_SK",gap_lines);
    {
        ProWVerstamp before=NULL,after=NULL;
        int same;
        ProMdlVerstampGet(model,&before);
        CASE(0,L"SKETCH_CHECK %ls GAP_SK",name);
        ProMdlVerstampGet(model,&after);
        same=before&&after&&ProVerstampEqual(before,after);
        trace("read-only audit preserves exact model version",same?0:-1);cases++;if(!same)failures++;
        if(before)ProVerstampFree(&before);
        if(after)ProVerstampFree(&after);
    }
    audit_check(model,L"GAP_SK",2,0,0);
    CASE(1,L"SKETCH_REPAIR %ls GAP_SK 0.02",name);
    CASE(0,L"SKETCH_REPAIR %ls GAP_SK 0.10",name);
    audit_check(model,L"GAP_SK",0,0,0);
    RAW(L"CROSS_SK",crossing_lines);
    CASE(0,L"SKETCH_CHECK %ls CROSS_SK",name);
    audit_check(model,L"CROSS_SK",8,1,1);
    CASE(1,L"SKETCH_REPAIR %ls CROSS_SK 0.20",name);
    expect_volume(model,96000);
done:
    trace("ADVANCED_SKETCH_CASES",cases);trace("ADVANCED_SKETCH_FAILURES",failures);trace("ADVANCED_SKETCH_DONE",1);
    ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
