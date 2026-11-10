#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"

static void check(int ok,const char *label){if(!ok)failures++;trace(label,ok?0:-1);}

static int probe_hole(ProMdl model,double x,double y){
    ProRay ray={{x,y,10},{0,0,1}};
    ProSelection *hits=NULL;
    int count,i;
    ProError e=ProSolidRayIntersectionCompute(model,0.1,&ray,&hits,&count);
    if(e==PRO_TK_E_NOT_FOUND)return 1;
    if(e)return 0;
    for(i=0;i<count;i++){
        ProModelitem item;
        Pro3dPnt point;
        if(!ProSelectionModelitemGet(hits[i],&item)&&item.type==PRO_SURFACE&&!ProSelectionPoint3dGet(hits[i],point)&&
           (fabs(point[2])<1e-6||fabs(point[2]-20)<1e-6)){
            ProSurface surface;
            ProBoolean datum=PRO_B_FALSE;
            if(!ProGeomitemToSurface(&item,&surface)&&!ProSurfaceIsDatumPlane((ProSolid)model,surface,&datum)&&!datum)return 0;
        }
    }
    return 1;
}

static int world_to_section(ProMdl model,const wchar_t *sketch_name,const double a[3],const double b[3],double local[4]){
    FdSketch sketch={0};ProMatrix transform;int i;
    ProError e=fd_sketch_open(model,sketch_name,&sketch,0);
    if(!e)e=ProSectionLocationGet(sketch.section,transform);
    if(!e)for(i=0;i<2;i++){
        int j;local[i]=local[i+2]=0;
        for(j=0;j<3;j++){
            local[i]+=(a[j]-transform[3][j])*transform[i][j];
            local[i+2]+=(b[j]-transform[3][j])*transform[i][j];
        }
    }
    fd_sketch_free(&sketch);
    return !e;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    const double pi=3.141592653589793;
    ProMdl model=NULL;ProMdlName name;Fixture f;ProError e;
    wchar_t cmd[1024];char marker[16];
    double drafted[2]={0};int side;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    model_background_mode=1;
#define NEW() do{if((e=fixture(&model,name,&f))){trace("fixture failure",e);failures++;goto done;}}while(0)
#define CASE(expected,fmt,...) do{swprintf_s(cmd,_countof(cmd),L"CREO_OP " fmt,__VA_ARGS__);run(model,cmd,expected);}while(0)
#define SEED(first,second) CASE(0,L"HOLE %ls SEED SURFACE:%d SURFACE:%d %g SURFACE:%d %g 6 THROUGH 0",name,f.top,f.x,(double)(first),f.y,(double)(second))
#define MID_PLANES() do{CASE(0,L"DATUM_PLANE %ls MID_X PLANE:RIGHT 40",name);CASE(0,L"DATUM_PLANE %ls MID_Y PLANE:TOP 30",name);}while(0)
    for(side=0;side<2;side++){
        NEW();
        CASE(0,L"SOLID_DRAFT %ls DRAFT_ONE SURFACE:%d SURFACE:%d PLANE:FRONT 5 %ls",name,f.xfar,f.bottom,side?L"NEG":L"POS");
        drafted[side]=volume(model);
        check(part_close(fabs(drafted[side]-96000),12000*tan(5*pi/180)),"draft analytic wedge");
        CASE(1,L"SOLID_DRAFT %ls BAD_DRAFT SURFACE:%d SURFACE:%d PLANE:RIGHT 5 POS",name,f.xfar,f.bottom);
    }
    check((drafted[0]-96000)*(drafted[1]-96000)<0,"draft directions opposite");

    NEW();
    CASE(0,L"DATUM_PLANE %ls MID_Y PLANE:TOP 30",name);
    {
        const double pillar_a[]={2,10,0},pillar_b[]={12,50,0},rib_a[]={12,30,44},rib_b[]={55,30,20};
        double local[4];
        CASE(0,L"SKETCH_CREATE %ls PILLAR_FRAME PLANE:FRONT PLANE:RIGHT LINE 0 0 2 0",name);
        if(!world_to_section(model,L"PILLAR_FRAME",pillar_a,pillar_b,local)){failures++;goto done;}
        CASE(0,L"SKETCH_CREATE %ls PILLAR_SK PLANE:FRONT PLANE:RIGHT RECT %.9g %.9g %.9g %.9g",name,
            fmin(local[0],local[2]),fmin(local[1],local[3]),fabs(local[0]-local[2]),fabs(local[1]-local[3]));
        CASE(0,L"SOLID_EXTRUDE %ls PILLAR PILLAR_SK ADD BLIND 50 NEG",name);
        expect_volume(model,108000);
        CASE(0,L"SKETCH_CREATE %ls RIB_FRAME PLANE:MID_Y PLANE:RIGHT LINE 0 0 2 0",name);
        if(!world_to_section(model,L"RIB_FRAME",rib_a,rib_b,local)){failures++;goto done;}
        CASE(0,L"SKETCH_CREATE %ls RIB_LINE PLANE:MID_Y PLANE:RIGHT LINE %.9g %.9g %.9g %.9g",name,local[0],local[1],local[2],local[3]);
        CASE(1,L"SOLID_RIB %ls WRONG_SIDE RIB_LINE 4 POS SYM",name);
        CASE(0,L"SOLID_RIB %ls RIB_ONE RIB_LINE 4 NEG SYM",name);
        expect_volume(model,110064);
    }
    CASE(0,L"SKETCH_CREATE %ls CLOSED_RIB PLANE:MID_Y PLANE:RIGHT CIRCLE 15 25 4",name);
    CASE(1,L"SOLID_RIB %ls BAD_RIB CLOSED_RIB 4 POS SYM",name);

    NEW();MID_PLANES();SEED(20,30);
    CASE(0,L"FEATURE_MIRROR %ls MIRRORED SEED PLANE:MID_X",name);
    expect_volume(model,96000-360*pi);
    check(probe_hole(model,20,30)&&probe_hole(model,60,30),"mirror measured centers");
    {ProFeature feature;ProFeattype type;
     if(!rf_active_feature(model,L"SEED",&feature,&type))CASE(1,L"FEATURE_DELETE %ls SEED FEATURE:%d",name,feature.id);
     else failures++;}

    NEW();SEED(20,30);
    CASE(0,L"DATUM_PLANE %ls THROUGH_SEED PLANE:RIGHT 20",name);
    CASE(1,L"FEATURE_MIRROR %ls OVERLAP SEED PLANE:THROUGH_SEED",name);

    for(side=0;side<2;side++){
        NEW();
        CASE(0,L"DATUM_PLANE %ls MID_X PLANE:RIGHT 40",name);
        CASE(0,L"DATUM_PLANE %ls TOP_Z PLANE:FRONT 20",name);
        CASE(0,L"SKETCH_CREATE %ls BOX_SK PLANE:TOP_Z PLANE:RIGHT RECT 15 24 10 12",name);
        CASE(0,L"SOLID_EXTRUDE %ls SEED BOX_SK %ls BLIND 8 %ls",name,side?L"CUT":L"ADD",side?L"POS":L"NEG");
        expect_volume(model,96000+(side?-960:960));
        CASE(0,L"FEATURE_MIRROR %ls MIRRORED SEED PLANE:MID_X",name);
        expect_volume(model,96000+(side?-1920:1920));
    }

    NEW();SEED(20,30);
    CASE(0,L"FEATURE_PATTERN_LINEAR %ls LINEAR SEED PLANE:RIGHT 3 15 POS",name);
    expect_volume(model,96000-540*pi);
    check(probe_hole(model,20,30)&&probe_hole(model,35,30)&&probe_hole(model,50,30),"linear measured centers");
    CASE(1,L"FEATURE_PATTERN_LINEAR %ls BAD_PATTERN SEED PLANE:RIGHT 2 15 POS",name);
    {ProFeature feature;ProFeattype type;
     if(!rf_active_feature(model,L"SEED",&feature,&type))CASE(1,L"FEATURE_DELETE %ls SEED FEATURE:%d",name,feature.id);
     else failures++;}

    NEW();SEED(50,30);
    CASE(0,L"FEATURE_PATTERN_LINEAR %ls REVERSE_LINE SEED PLANE:RIGHT 3 15 NEG",name);
    expect_volume(model,96000-540*pi);
    check(probe_hole(model,50,30)&&probe_hole(model,35,30)&&probe_hole(model,20,30),"reverse linear measured centers");

    NEW();SEED(60,30);
    CASE(1,L"FEATURE_PATTERN_LINEAR %ls OUTSIDE SEED PLANE:RIGHT 3 15 POS",name);
    CASE(0,L"FEATURE_PATTERN_LINEAR %ls RETRY SEED PLANE:RIGHT 3 15 NEG",name);
    expect_volume(model,96000-540*pi);

    NEW();MID_PLANES();
    CASE(0,L"DATUM_AXIS %ls CENTER_AXIS PLANES PLANE:MID_X PLANE:MID_Y",name);
    SEED(55,30);
    CASE(0,L"FEATURE_PATTERN_CIRCULAR %ls CIRCULAR SEED AXIS:CENTER_AXIS 4 90 POS",name);
    expect_volume(model,96000-720*pi);
    check(probe_hole(model,55,30)&&probe_hole(model,40,45)&&probe_hole(model,25,30)&&probe_hole(model,40,15),"circular measured centers");

    {
        int side_at_y[2]={0};
        for(side=0;side<2;side++){
            NEW();MID_PLANES();
            CASE(0,L"DATUM_AXIS %ls CENTER_AXIS PLANES PLANE:MID_X PLANE:MID_Y",name);
            SEED(55,30);
            CASE(0,L"FEATURE_PATTERN_CIRCULAR %ls DIR_ARC SEED AXIS:CENTER_AXIS 2 90 %ls",name,side?L"NEG":L"POS");
            expect_volume(model,96000-360*pi);
            side_at_y[side]=probe_hole(model,40,45)-probe_hole(model,40,15);
        }
        check(side_at_y[0]*side_at_y[1]==-1,"circle directions yield opposite measured centers");
    }

    NEW();SEED(20,30);
    {ProFeature feature;ProFeattype type;
     if(!rf_active_feature(model,L"SEED",&feature,&type)){
         CASE(1,L"FEATURE_DELETE %ls SEED FEATURE:%d",name,feature.id+1);
         CASE(0,L"FEATURE_DELETE %ls SEED FEATURE:%d",name,feature.id);
         expect_volume(model,96000);
     }else failures++;}
done:
    trace("REFINEMENT_CASES",cases);trace("REFINEMENT_FAILURES",failures);trace("REFINEMENT_DONE",1);
    ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
