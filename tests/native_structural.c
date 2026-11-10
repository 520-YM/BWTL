#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
static void md_check(int pass,const char *label){trace(label,pass?0:-1);cases++;if(!pass)failures++;}
static ProError md_test_cylinder(ProSurface surface,ProError status,ProAppData data){
    ProSrftype type;if(status)return status;
    if(!ProSurfaceTypeGet(surface,&type)&&type==PRO_SRF_CYL)return ProSurfaceIdGet(surface,(int *)data);return 0;
}
static double md_max_x(ProMdl model,double y,double z){
    ProRay ray={{-100,y,z},{1,0,0}};ProSelection *hits=NULL;int count=0;double maximum=-1e10;ProError e;
    e=ProSolidRayIntersectionCompute(model,0.01,&ray,&hits,&count);
    if(!e)for(int i=0;i<count;i++){
        ProModelitem item;ProSurface surface;ProQuilt quilt;ProBoolean datum;Pro3dPnt p;
        if(ProSelectionModelitemGet(hits[i],&item)||item.type!=PRO_SURFACE||ProGeomitemToSurface(&item,&surface)||
           ProSurfaceIsDatumPlane((ProSolid)model,surface,&datum)||datum||!ProSurfaceQuiltGet((ProSolid)model,surface,&quilt)||ProSelectionPoint3dGet(hits[i],p))continue;
        maximum=fmax(maximum,p[0]);
    }
    /* ProSolidRayIntersectionCompute owns and reuses this result buffer. */
    return maximum;
}
static int md_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;ProMdlName name;Fixture f;ProError e;wchar_t text[1600];
    model_background_mode=1;
#define NEW() do{if((e=fixture(&model,name,&f))){trace("structural fixture",e);failures++;goto done;}}while(0)
#define CASE(reject,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),run(model,text,reject))
    NEW();
    CASE(0,L"SHELL_MULTI %ls TUBE 2 SURFACE:%d SURFACE:%d",name,f.top,f.bottom);
    expect_volume(model,10880);
    CASE(1,L"SHELL_MULTI %ls BAD_OPENING 2 SURFACE:999999 SURFACE:%d",name,f.xfar);
    NEW();
    CASE(0,L"SHELL_LOCAL %ls THICK_BOTTOM 2 SURFACE:%d SURFACE:%d 4",name,f.top,f.bottom);
    expect_volume(model,27904);
    NEW();
    CASE(0,L"SHELL_LOCAL %ls TWO_LOCAL 2 SURFACE:%d SURFACE:%d 4 SURFACE:%d 3",name,f.top,f.bottom,f.xfar);
    expect_volume(model,28800);
    NEW();
    CASE(0,L"SHELL_MULTI %ls THREE_OPEN 2 SURFACE:%d SURFACE:%d SURFACE:%d",name,f.top,f.bottom,f.xfar);
    NEW();
    CASE(0,L"DRAFT_MULTI %ls TWO_FACES PLANE:FRONT PLANE:FRONT 5 POS SURFACE:%d SURFACE:%d",name,f.xfar,f.yfar);
    NEW();
    CASE(0,L"DATUM_PLANE %ls MID_Z PLANE:FRONT 10",name);
    CASE(0,L"DRAFT_SPLIT %ls SPLIT PLANE:MID_Z PLANE:FRONT 5 3 POS SURFACE:%d",name,f.xfar);
    {double low=fabs(md_max_x(model,30,1)-80)/9,high=fabs(md_max_x(model,30,19)-80)/9;
     {FILE *log=fopen("draft-geometry.log","a");if(log){fprintf(log,"split slopes %g %g\n",low,high);fclose(log);}}
     md_check((fabs(low-tan(5*AS_PI/180))<0.0005&&fabs(high-tan(3*AS_PI/180))<0.0005)||
              (fabs(high-tan(5*AS_PI/180))<0.0005&&fabs(low-tan(3*AS_PI/180))<0.0005),"verify actual split draft wall slopes");}
    NEW();
    CASE(0,L"DATUM_POINT %ls A CSYS:PRT_CSYS_DEF 80 0 0",name);
    CASE(0,L"DATUM_POINT %ls B CSYS:PRT_CSYS_DEF 80 60 0",name);
    CASE(0,L"DATUM_POINT %ls A_COPY CSYS:PRT_CSYS_DEF 80 0 0",name);
    CASE(1,L"DRAFT_VARIABLE %ls SAME_STATION SURFACE:%d PLANE:FRONT PLANE:FRONT POINT:A 2 POINT:A_COPY 5 POS",name,f.xfar);
    CASE(0,L"DRAFT_VARIABLE %ls VARIABLE SURFACE:%d PLANE:FRONT PLANE:FRONT POINT:A 2 POINT:B 5 POS",name,f.xfar);
    {double a=md_max_x(model,0.5,19),b=md_max_x(model,59.5,19);
     FILE *log=fopen("draft-geometry.log","a");if(log){fprintf(log,"variable x %g %g\n",a,b);fclose(log);}
     md_check(fabs(fabs(a-80)-19*tan(2*AS_PI/180))<0.03&&
             fabs(fabs(b-80)-19*tan(5*AS_PI/180))<0.03,"verify actual variable draft end angles");}
    NEW();
    CASE(0,L"DATUM_PLANE %ls RIB_PLANE PLANE:FRONT 20",name);
    CASE(0,L"SKETCH_CREATE %ls RIB_PATH PLANE:RIB_PLANE PLANE:RIGHT LINE 20 30 60 30",name);
    CASE(1,L"DRAFT_PATH_RIB %ls INVALID_TOP CURVE:RIB_PATH SURFACE:%d 1 8 30",name,f.top);
    CASE(1,L"DRAFT_PATH_RIB %ls OFF_PLANE CURVE:RIB_PATH PLANE:FRONT 4 8 3",name);
    CASE(0,L"SKETCH_CREATE %ls OUTSIDE_PATH PLANE:RIB_PLANE PLANE:RIGHT LINE 200 30 240 30",name);
    CASE(1,L"DRAFT_PATH_RIB %ls DISCONNECTED CURVE:OUTSIDE_PATH SURFACE:%d 4 8 3",name,f.top);
    CASE(0,L"DRAFT_PATH_RIB %ls RIB CURVE:RIB_PATH SURFACE:%d 4 8 3",name,f.top);
    expect_volume(model,96000+(4-8*tan(3*AS_PI/180))*8*40);
    NEW();
    CASE(0,L"DATUM_CSYS %ls BOSS_AT CSYS:PRT_CSYS_DEF 40 30 20 0 0 0",name);
    CASE(0,L"SCREW_BOSS %ls BOSS SURFACE:%d CSYS:BOSS_AT 12 10 4 8",name,f.top);
    expect_volume(model,96000+328*AS_PI);
    NEW();
    CASE(0,L"DATUM_CSYS %ls SOLID_AT CSYS:PRT_CSYS_DEF 40 30 20 0 0 0",name);
    CASE(0,L"SCREW_BOSS %ls SOLID_BOSS SURFACE:%d CSYS:SOLID_AT 12 10 0 0",name,f.top);
    expect_volume(model,96000+360*AS_PI);
    NEW();
    CASE(0,L"DATUM_CSYS %ls THRU_AT CSYS:PRT_CSYS_DEF 40 30 20 0 0 0",name);
    CASE(0,L"SCREW_BOSS %ls THRU_BOSS SURFACE:%d CSYS:THRU_AT 12 10 4 10",name,f.top);
    expect_volume(model,96000+320*AS_PI);
    NEW();
    CASE(0,L"DATUM_CSYS %ls HOOK_AT CSYS:PRT_CSYS_DEF 80 30 10 0 0 0",name);
    CASE(0,L"SNAP_HOOK %ls HOOK SURFACE:%d CSYS:HOOK_AT 20 6 2 1 4",name,f.xfar);
    expect_volume(model,96252);
    NEW();
    CASE(0,L"DATUM_CSYS %ls WIN_AT CSYS:PRT_CSYS_DEF 80 30 10 0 0 0",name);
    CASE(0,L"SNAP_WINDOW %ls WINDOW SURFACE:%d CSYS:WIN_AT 6 2 1 0.2 80",name,f.xfar);
    expect_volume(model,96000-6.4*3.4*80);
    NEW();
    CASE(0,L"DATUM_CSYS %ls SEAL_AT CSYS:PRT_CSYS_DEF 40 30 20 0 0 0",name);
    CASE(1,L"SEAL_GROOVE_FACE %ls CLIPPED_SEAL SURFACE:%d CSYS:SEAL_AT 70 3 20 4",name,f.top);
    CASE(0,L"SEAL_GROOVE_FACE %ls SEAL SURFACE:%d CSYS:SEAL_AT 30 3 20 4",name,f.top);
    expect_volume(model,96000-288*AS_PI);
    CASE(1,L"SEAL_GROOVE_FACE %ls OUTSIDE_SEAL SURFACE:999999 CSYS:SEAL_AT 30 3 20 4",name);
    {ModelPlan p={1,40,40,40,0,1};const char *stage="radial seal fixture";int cylinder=0;
     if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){failures++;goto done;}
     if(ProSolidSurfaceVisit((ProSolid)model,md_test_cylinder,NULL,&cylinder)||!cylinder){failures++;goto done;}
     CASE(0,L"DATUM_CSYS %ls SEAL_AXIS CSYS:PRT_CSYS_DEF 20 20 0 0 0 0",name);
     CASE(0,L"SEAL_GROOVE_RADIAL %ls RADIAL_SEAL SHAFT SURFACE:%d CSYS:SEAL_AXIS 10 2 25 3",name,cylinder);
     expect_volume(model,16000*AS_PI-173.25*AS_PI);
    }
    NEW();
    CASE(1,L"STOCK_ALLOWANCE %ls BAD_STOCK 2 SURFACE:999999",name);
    {ProMdl bad;ProMdlName target=L"BAD_STOCK";md_check(ProMdlnameInit(target,PRO_MDL_PART,&bad)!=0,"verify failed stock copy erased");}
    CASE(0,L"STOCK_ALLOWANCE %ls STOCK_TEST 2 SURFACE:%d",name,f.top);
    {ProMdl copy;ProMdlName target=L"STOCK_TEST";
     if(ProMdlnameInit(target,PRO_MDL_PART,&copy)){failures++;}else expect_volume(copy,105600);
     expect_volume(model,96000);}
    CASE(1,L"STOCK_ALLOWANCE %ls STOCK_TEST 2 SURFACE:%d",name,f.top);
    CASE(0,L"STOCK_ALLOWANCE %ls STOCK_TWO_FACES 2 SURFACE:%d SURFACE:%d",name,f.top,f.xfar);
    {ProMdl copy;ProMdlName target=L"STOCK_TWO_FACES";if(ProMdlnameInit(target,PRO_MDL_PART,&copy)){failures++;}else expect_volume(copy,108240);expect_volume(model,96000);}
done:trace("STRUCTURAL_CASES",cases);trace("STRUCTURAL_FAILURES",failures);trace("STRUCTURAL_DONE",1);ProEngineerEnd();return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;char marker[16];ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLStructuralProbe",md_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;
    return ProMacroLoad(L"~ Command `BWTLStructuralProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
