#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
static ProError am_test_cyl(ProSurface surface,ProError status,ProAppData data){
    ProSrftype type;if(status)return status;
    if(!ProSurfaceTypeGet(surface,&type)&&type==PRO_SRF_CYL)return ProSurfaceIdGet(surface,(int *)data);
    return 0;
}
static int am_test_side(ProMdl model,int id){
    ProEdge edge,next[2];ProSurface side[2];int result=0;
    if(!ProEdgeInit((ProSolid)model,id,&edge)&&!ProEdgeNeighborsGet(edge,&next[0],&next[1],&side[0],&side[1]))ProSurfaceIdGet(side[0],&result);
    return result;
}
typedef struct {double radius;int id;} AmTestCyl;
static ProError am_test_radius(ProSurface surface,ProError status,ProAppData data){
    AmTestCyl *c=data;ProSrftype type;ProGeomitemdata *raw=NULL;ProError e;
    if(status)return status;
    if((e=ProSurfaceTypeGet(surface,&type)))return e;
    if(type!=PRO_SRF_CYL)return 0;
    if((e=ProSurfaceDataGet(surface,&raw)))return e;
    if(part_close(raw->data.p_surface_data->srf_shape.cylinder.radius,c->radius))e=ProSurfaceIdGet(surface,&c->id);
    ProGeomitemdataFree(&raw);return e;
}
static ProError am_test_top_edge(ProGeomitem *item,ProError status,ProAppData data){
    ProEdge edge;ProVector a,b;ProEnttype type;if(status)return status;
    if(ProGeomitemToEdge(item,&edge)||ProEdgeTypeGet(edge,&type)||type!=PRO_ENT_LINE)return 0;
    if(ProEdgeXyzdataEval(edge,0,a,NULL,NULL,NULL)||ProEdgeXyzdataEval(edge,1,b,NULL,NULL,NULL))return PRO_TK_GENERAL_ERROR;
    if(fabs(a[2])<1e-6&&fabs(b[2])<1e-6)*(int *)data=item->id;
    return 0;
}
static ProError am_test_cone(ProSurface surface,ProError status,ProAppData data){
    ProSrftype type;if(status)return status;
    if(!ProSurfaceTypeGet(surface,&type)&&type==PRO_SRF_CONE)return ProSurfaceIdGet(surface,(int *)data);
    return 0;
}
static void am_expect_removed(ProMdl model,double before,double expected){
    double removed=before-volume(model);
    /* Oblique face integration in this Creo template has finite accuracy.
       Bound error by removed material, not by the much larger stock volume. */
    int pass=fabs(removed-expected)<=fmax(0.02,expected*0.0001);
    trace("verify analytic removed volume within native integration accuracy",pass?0:-1);cases++;if(!pass)failures++;
}
static int am_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;ProMdlName name;Fixture f;ProError e;wchar_t text[1800];int cylinder=0;
    model_background_mode=1;
#define NEW() do{if((e=fixture(&model,name,&f))){trace("fixture",e);failures++;goto done;}}while(0)
#define CASE(reject,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),run(model,text,reject))
    NEW();
    CASE(0,L"HOLE_THREAD_THRU %ls THRU_ISO SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 ISO M6x1 THROUGH 0 0",name,f.top,f.x,f.y);
    NEW();
    CASE(0,L"HOLE_THREAD_STD %ls THRU_UNC SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 UNC 1/4-20 THROUGH 0 0",name,f.top,f.x,f.y);
    NEW();
    CASE(0,L"HOLE_THREAD_STD %ls BLIND_UNF SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 UNF 1/4-28 BLIND 8 12",name,f.top,f.x,f.y);
    NEW();
    CASE(0,L"EDGE_ROUND_VARIABLE %ls VARIABLE EDGE:%d 1 3",name,f.vertical[0]);
    NEW();
    CASE(0,L"EDGE_ROUND_TANGENT %ls CHAIN EDGE:%d 2",name,f.vertical[0]);
    NEW();
    CASE(0,L"EDGE_CHAMFER_ADV %ls UNEQUAL EDGE:%d SURFACE:%d DD 2 4",name,f.vertical[0],am_test_side(model,f.vertical[0]));
    expect_volume(model,95920);
    NEW();
    CASE(0,L"EDGE_CHAMFER_ADV %ls ANGLE EDGE:%d SURFACE:%d AD 30 3",name,f.vertical[0],am_test_side(model,f.vertical[0]));
    expect_volume(model,96000-90*tan(AS_PI/6));
    NEW();
    CASE(0,L"DATUM_POINT %ls START CSYS:PRT_CSYS_DEF 40 30 20",name);
    CASE(0,L"DATUM_POINT %ls ENDPT CSYS:PRT_CSYS_DEF 45 30 0",name);
    CASE(0,L"DATUM_AXIS_2POINT %ls HOLE_AXIS POINT:START POINT:ENDPT",name);
    CASE(0,L"HOLE_AXIS %ls OBLIQUE POINT:START AXIS:HOLE_AXIS 6 THROUGH 0 POS",name);
    am_expect_removed(model,96000,AS_PI*9*20*sqrt(1.0625));
    CASE(1,L"HOLE_AXIS %ls STALE_AXIS POINT:START AXIS:MISSING 6 THROUGH 0 POS",name);
    NEW();
    CASE(0,L"DATUM_POINT %ls START CSYS:PRT_CSYS_DEF 40 30 20",name);
    CASE(0,L"DATUM_POINT %ls ENDPT CSYS:PRT_CSYS_DEF 45 30 0",name);
    CASE(0,L"DATUM_AXIS_2POINT %ls HOLE_AXIS POINT:START POINT:ENDPT",name);
    CASE(0,L"SPOTFACE %ls SEAT POINT:START AXIS:HOLE_AXIS 12 4 POS",name);
    am_expect_removed(model,96000,36*AS_PI*4);
    {ModelPlan p={1,40,40,40,0,1};const char *stage="radial fixture";
     if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}}
    if((e=ProSolidSurfaceVisit((ProSolid)model,am_test_cyl,NULL,&cylinder))||!cylinder){failures++;goto done;}
    CASE(0,L"HOLE_RADIAL %ls RADIAL SURFACE:%d PLANE:RIGHT 0 PLANE:FRONT 20 6 BLIND 6",name,cylinder);
    CASE(1,L"HOLE_RADIAL %ls BAD_SURFACE SURFACE:999999 PLANE:RIGHT 0 PLANE:FRONT 20 6 BLIND 6",name);
    CASE(0,L"ADV_MACHINING_INFO %ls",name);
    for(int groove=0;groove<4;groove++){
        int hub=groove>=2;ModelPlan p={1,40,40,40,hub?20:0,1};const char *stage="groove fixture";
        if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){failures++;goto done;}
        AmTestCyl pick={hub?10:20,0};ProSolidSurfaceVisit((ProSolid)model,am_test_radius,NULL,&pick);cylinder=pick.id;
        CASE(0,L"DATUM_CSYS %ls SHAFT_CSYS CSYS:PRT_CSYS_DEF 20 20 0 0 0 0",name);
        if(groove%2)CASE(0,L"RING_GROOVE %ls RING %ls SURFACE:%d CSYS:SHAFT_CSYS 10 2 1",name,hub?L"HUB":L"SHAFT",cylinder);
        else CASE(0,L"KEYWAY %ls KEY %ls SURFACE:%d CSYS:SHAFT_CSYS 5 6 25 3",name,hub?L"HUB":L"SHAFT",cylinder);
        CASE(1,L"KEYWAY %ls OUTSIDE %ls SURFACE:%d CSYS:SHAFT_CSYS 35 6 25 3",name,hub?L"HUB":L"SHAFT",cylinder);
    }
    {const char *stage="tangent chain fixture";ProFeature feature;int edge=0;
     if((e=model_create_empty(1,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){failures++;goto done;}
     CASE(0,L"SKETCH_CREATE %ls SLOT_SK PLANE:FRONT PLANE:RIGHT SLOT 0 0 40 20 0",name);
     CASE(0,L"SOLID_EXTRUDE %ls TANGENT_BASE SLOT_SK ADD BLIND 10 POS",name);
     if(ProModelitemByNameInit(model,PRO_FEATURE,L"TANGENT_BASE",(ProModelitem *)&feature)||ProFeatureGeomitemVisit(&feature,PRO_EDGE,am_test_top_edge,NULL,&edge)||!edge){failures++;goto done;}
     CASE(0,L"EDGE_ROUND_TANGENT %ls CLOSED_CHAIN EDGE:%d 2",name,edge);
    }
    NEW();
    CASE(1,L"HOLE_THREAD_STD %ls INVALID_SIZE SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 UNC 1/4-99 THROUGH 0 0",name,f.top,f.x,f.y);
    CASE(0,L"HOLE_THREAD_THRU %ls FINE_ISO SURFACE:%d SURFACE:%d 20 SURFACE:%d 20 ISO M6x0.75 THROUGH 0 0",name,f.top,f.x,f.y);
    NEW();
    CASE(1,L"EDGE_ROUND_VARIABLE %ls BAD_RADIUS EDGE:999999 1 3",name);
    CASE(1,L"EDGE_CHAMFER_ADV %ls DATUM_NOT_FACE EDGE:%d SURFACE:%d DD 2 4",name,f.vertical[0],f.x);
    CASE(0,L"EDGE_ROUND_VARIABLE %ls REVERSED_RADII EDGE:%d 3 1",name,f.vertical[0]);
    NEW();
    CASE(0,L"DATUM_POINT %ls START CSYS:PRT_CSYS_DEF 40 30 0",name);
    CASE(0,L"DATUM_POINT %ls ENDPT CSYS:PRT_CSYS_DEF 45 30 -20",name);
    CASE(0,L"DATUM_AXIS_2POINT %ls AXIS_NEG POINT:START POINT:ENDPT",name);
    CASE(0,L"HOLE_AXIS %ls REVERSED_HOLE POINT:START AXIS:AXIS_NEG 6 BLIND 8 NEG",name);
    am_expect_removed(model,96000,9*AS_PI*8);
    NEW();
    CASE(0,L"DATUM_POINT %ls START CSYS:PRT_CSYS_DEF 40 30 20",name);
    CASE(0,L"DATUM_POINT %ls ENDPT CSYS:PRT_CSYS_DEF 45 30 0",name);
    CASE(0,L"DATUM_AXIS_2POINT %ls SEAT_AXIS POINT:START POINT:ENDPT",name);
    CASE(1,L"SPOTFACE %ls PARTIAL_SEAT POINT:START AXIS:SEAT_AXIS 12 0.1 POS",name);
    CASE(0,L"SPOTFACE %ls RECOVERED_SEAT POINT:START AXIS:SEAT_AXIS 12 4 POS",name);
    {ModelPlan p={1,40,40,40,0,1};const char *stage="curved spotface";
     if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){failures++;goto done;}}
    CASE(0,L"DATUM_POINT %ls START CSYS:PRT_CSYS_DEF 40 20 10",name);
    CASE(0,L"DATUM_POINT %ls ENDPT CSYS:PRT_CSYS_DEF 0 20 10",name);
    CASE(0,L"DATUM_AXIS_2POINT %ls SEAT_AXIS POINT:START POINT:ENDPT",name);
    {FdRef axis={0};ProVector p,n;
     if(fd_ref_resolve(model,L"AXIS:SEAT_AXIS",PRO_AXIS,&axis)||ad_line(&axis,p,n)){failures++;goto done;}
     CASE(0,L"SPOTFACE %ls CURVED_SEAT POINT:START AXIS:SEAT_AXIS 12 4 %ls",name,n[0]>0?L"NEG":L"POS");fd_ref_free(&axis);}
    {const char *stage="conical entrance";
     if((e=model_create_empty(1,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){failures++;goto done;}}
    CASE(0,L"SKETCH_CREATE %ls C1 PLANE:FRONT PLANE:RIGHT CIRCLE 0 0 40",name);
    CASE(0,L"DATUM_PLANE %ls END_PLANE PLANE:FRONT 40",name);
    CASE(0,L"SKETCH_CREATE %ls C2 PLANE:END_PLANE PLANE:RIGHT CIRCLE 0 0 20",name);
    CASE(0,L"SOLID_BLEND %ls CONE C1 C2",name);
    cylinder=0;ProSolidSurfaceVisit((ProSolid)model,am_test_cone,NULL,&cylinder);
    if(!cylinder){failures++;goto done;}
    CASE(0,L"HOLE_RADIAL %ls CONE_HOLE SURFACE:%d PLANE:RIGHT 30 PLANE:FRONT 20 4 BLIND 5",name,cylinder);
done:
    trace("ADV_MACHINING_CASES",cases);trace("ADV_MACHINING_FAILURES",failures);trace("ADV_MACHINING_DONE",1);
    ProEngineerEnd();return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;ProError e;char marker[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLAdvancedMachiningProbe",am_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;
    return ProMacroLoad(L"~ Command `BWTLAdvancedMachiningProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
