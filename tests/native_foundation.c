#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
#include <ProUICmd.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int e){FILE *f=fopen("foundation-result.log","a");if(f){fprintf(f,"%s: %d\n",label,e);fclose(f);}}
#include "../src/native_model.h"
#include "../src/part_tools.h"
static int failures=0;
static ProError prepare_weak_radius(ProMdl model,const wchar_t *name,int entity){
    FdSketch s={0};ProError e=fd_sketch_open(model,name,&s,0);int *all=NULL,n=0,i,found=-1;ProErrorlist errors={0};
    if(e)goto done;if((e=ProSecdimIdsGet(s.section,&all,&n)))goto done;
    for(i=0;i<n;i++){
        ProSecdimType type;int *ids=NULL,count=0;ProSectionPointType *senses=NULL;
        if(!ProSecdimTypeGet(s.section,all[i],&type)&&(type==PRO_TK_DIM_DIA||type==PRO_TK_DIM_RAD)&&!ProSecdimReferencesGet(s.section,all[i],&ids,&senses,&count)&&count==1&&ids[0]==entity)found=all[i];
        if(ids)ProArrayFree((ProArray *)&ids);if(senses)ProArrayFree((ProArray *)&senses);
    }
    if(found<0){e=PRO_TK_E_NOT_FOUND;goto done;}if((e=ProSecdimDelete(s.section,found)))goto done;
    if((e=fd_section_solve(s.section,1)))goto done;s.options[0]=PRO_FEAT_CR_NO_OPTS;
    e=ProFeatureWithoptionsRedefine(NULL,&s.feature,s.tree,s.options,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);if(!e)e=part_regenerate(model);
done:if(all)ProArrayFree((ProArray *)&all);fd_sketch_free(&s);trace("prepare private weak radius for equal-radius test",e);if(e)failures++;return e;
}
static ProError run(const wchar_t *protocol,ProMdl model){
    PartCommand cmd={0};wchar_t message[PART_TEXT_MAX]=L"";FILE *f;ProError e;
    if(part_command_parse(protocol,&cmd)!=1){e=PRO_TK_BAD_INPUTS;wcscpy_s(message,PART_TEXT_MAX,L"协议校验拒绝，未调用Creo写接口。");}
    else e=foundation_execute(&cmd,model,message,PART_TEXT_MAX);
    f=_wfopen(L"foundation-cases.log",L"a, ccs=UTF-8");if(f){fwprintf(f,L"%ls\nRESULT=%d\n%ls\n",protocol,e,message);fclose(f);}
    if(e)failures++;return e;
}
static void reject(const wchar_t *protocol,ProMdl model){
    ProWVerstamp before=NULL,after=NULL;ProError e;int prior=failures;
    if(ProMdlVerstampGet(model,&before)){failures++;return;}
    e=run(protocol,model);if(e)failures=prior;else failures++;
    if(ProMdlVerstampGet(model,&after)||!ProVerstampEqual(before,after))failures++;
    if(before)ProVerstampFree(&before);if(after)ProVerstampFree(&after);trace("expected rejection leaves model stamp unchanged",failures==prior?0:-1);
}
static void verify_coordinates(ProMdl model){
    FdRef source={0},point={0},csys={0},curve={0};ProCsys frame,created;ProGeomitemdata *a=NULL,*b=NULL;ProVector p,d1,d2;ProPoint pt;ProCurve crv;int i;ProError e=0;
    if((e=fd_ref_resolve(model,L"CSYS:PRT_CSYS",PRO_CSYS,&source))||(e=fd_ref_resolve(model,L"POINT:TEST_POINT",PRO_POINT,&point))||(e=fd_ref_resolve(model,L"CSYS:TEST_CSYS",PRO_CSYS,&csys))||(e=fd_ref_resolve(model,L"CURVE:TEST_CURVE",PRO_CURVE,&curve)))goto done;
    if((e=ProGeomitemToCsys(&source.item,&frame))||(e=ProCsysDataGet(frame,&a))||(e=ProGeomitemToPoint(&point.item,&pt))||(e=ProPointCoordGet(pt,p)))goto done;
    for(i=0;i<3;i++){double expected=a->data.p_csys_data->origin[i]+10*a->data.p_csys_data->x_vector[i]+20*a->data.p_csys_data->y_vector[i]+30*a->data.p_csys_data->z_vector[i];if(!part_close(p[i],expected)){e=PRO_TK_GENERAL_ERROR;goto done;}}
    trace("point XYZ matches actual source coordinate system",0);
    if((e=ProGeomitemToCsys(&csys.item,&created))||(e=ProCsysDataGet(created,&b)))goto done;
    for(i=0;i<3;i++){
        double origin=a->data.p_csys_data->origin[i]+10*a->data.p_csys_data->x_vector[i]+20*a->data.p_csys_data->y_vector[i]+30*a->data.p_csys_data->z_vector[i];
        double x=cos(3.141592653589793/6)*a->data.p_csys_data->x_vector[i]+sin(3.141592653589793/6)*a->data.p_csys_data->y_vector[i];
        double y=-sin(3.141592653589793/6)*a->data.p_csys_data->x_vector[i]+cos(3.141592653589793/6)*a->data.p_csys_data->y_vector[i];
        if(!part_close(b->data.p_csys_data->origin[i],origin)||!part_close(b->data.p_csys_data->x_vector[i],x)||!part_close(b->data.p_csys_data->y_vector[i],y)||!part_close(b->data.p_csys_data->z_vector[i],a->data.p_csys_data->z_vector[i])){e=PRO_TK_GENERAL_ERROR;trace("csys coordinate mismatch",i);goto done;}
    }trace("csys translation and 30 degree Z rotation verified",0);
    if((e=ProGeomitemToCurve(&curve.item,&crv))||(e=ProCurveXyzdataEval(crv,0,p,d1,d2)))goto done;
    for(i=0;i<3;i++)if(!part_close(p[i],a->data.p_csys_data->origin[i])){e=PRO_TK_GENERAL_ERROR;goto done;}
    if((e=ProCurveXyzdataEval(crv,1,p,d1,d2)))goto done;
    for(i=0;i<3;i++){double expected=a->data.p_csys_data->origin[i]+30*a->data.p_csys_data->x_vector[i]+20*a->data.p_csys_data->y_vector[i]+10*a->data.p_csys_data->z_vector[i];if(!part_close(p[i],expected)){e=PRO_TK_GENERAL_ERROR;goto done;}}
    trace("curve endpoints verified in actual source coordinate system",0);
done:trace("coordinate and curve numerical verification",e);if(e)failures++;if(a)ProGeomitemdataFree(&a);if(b)ProGeomitemdataFree(&b);fd_ref_free(&source);fd_ref_free(&point);fd_ref_free(&csys);fd_ref_free(&curve);
}
static ProError find_cylinder(ProGeomitem *item,ProError status,ProAppData data){
    ProSurface surface;ProSrftype type;if(status)return status;
    if(!ProGeomitemToSurface(item,&surface)&&!ProSurfaceTypeGet(surface,&type)&&type==PRO_SRF_CYL)*(int *)data=item->id;return 0;
}
static ProError find_cylinder_feature(ProFeature *feature,ProError status,ProAppData data){
    ProError e;if(status)return status;e=ProFeatureGeomitemVisit(feature,PRO_SURFACE,find_cylinder,NULL,data);return e==PRO_TK_E_NOT_FOUND?0:e;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProMdl model=NULL;ProMdlName name;const char *stage="new template";ProError e;wchar_t cmd[1024];char marker[16];
    trace("ENTER_INITIALIZE",0);
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    trace("FOUNDATION_SELFTEST_STARTED",0);model_background_mode=1;
    e=model_create_empty(1,name,&stage);trace(stage,e);if(e)goto done;
    e=ProMdlnameInit(name,PRO_MDL_PART,&model);trace("resolve test model",e);if(e)goto done;
#define CASE(tail) do{swprintf_s(cmd,1024,L"CREO_OP %ls %ls %ls",tail[0],name,tail[1]);run(cmd,model);}while(0)
    {const wchar_t *t[]={L"REFERENCES",L""};CASE(t);}
    {const wchar_t *t[]={L"DATUM_PLANE",L"TEST_PLANE PLANE:TOP 25"};CASE(t);}
    {const wchar_t *t[]={L"DATUM_AXIS",L"TEST_AXIS PLANES PLANE:TOP PLANE:RIGHT"};CASE(t);}
    {const wchar_t *t[]={L"DATUM_POINT",L"TEST_POINT CSYS:PRT_CSYS 10 20 30"};CASE(t);}
    {const wchar_t *t[]={L"DATUM_CSYS",L"TEST_CSYS CSYS:PRT_CSYS 10 20 30 0 0 30"};CASE(t);}
    {const wchar_t *t[]={L"DATUM_CURVE",L"TEST_CURVE CSYS:PRT_CSYS 0 0 0 30 20 10"};CASE(t);}
    verify_coordinates(model);
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_LINE PLANE:TOP PLANE:RIGHT LINE 0 0 40 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_RECT PLANE:TOP PLANE:RIGHT RECT 0 0 80 60"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_CIRCLE PLANE:TOP PLANE:RIGHT CIRCLE 0 0 10"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_ARC PLANE:TOP PLANE:RIGHT ARC 0 0 10 0 90"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_INFO",L"TEST_LINE"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_INFO",L"TEST_RECT"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_LINE LENGTH 0 NONE 55"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_CIRCLE DIAMETER 0 NONE 30"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_ARC RADIUS 0 NONE 15"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_LINE HORIZONTAL 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_LINE LINE 10 20 40 35"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_LINE PARALLEL 0 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_INFO",L"TEST_LINE"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_ANGLE PLANE:TOP PLANE:RIGHT LINE 0 0 40 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_ANGLE LINE 40 0 60 20"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_ANGLE ANGLE 0 1 60"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_DIST PLANE:TOP PLANE:RIGHT CIRCLE 10 10 5"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_DIST CIRCLE 40 20 7"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_DIST DISTANCE_X 0:CENTER 1:CENTER 35"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_DIST DISTANCE_Y 0:CENTER 1:CENTER 12"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_VERT PLANE:TOP PLANE:RIGHT LINE 0 0 20 30"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_VERT VERTICAL 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_ORTHO PLANE:TOP PLANE:RIGHT LINE 0 0 40 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_ORTHO LINE 0 20 30 35"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_ORTHO PERPENDICULAR 0 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_EQUAL PLANE:TOP PLANE:RIGHT LINE 0 0 40 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_EQUAL LINE 10 20 40 35"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_EQUAL EQUAL_LENGTH 0 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_EQUAL COINCIDENT 0:END 1:START"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_TANGENT PLANE:TOP PLANE:RIGHT CIRCLE 0 0 10"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_TANGENT LINE 0 20 40 20"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_TANGENT TANGENT 0 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_SYMMETRY PLANE:TOP PLANE:RIGHT LINE -10 10 -10 30"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_SYMMETRY LINE 10 5 10 35"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_SYMMETRY CENTERLINE 0 -40 0 40"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_SYMMETRY SYMMETRY 0:START 1:START 2"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_RADII PLANE:TOP PLANE:RIGHT CIRCLE 0 0 10"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_RADII CIRCLE 30 20 5"};CASE(t);}
    prepare_weak_radius(model,L"TEST_RADII",1);
    {const wchar_t *t[]={L"SKETCH_CONSTRAINT",L"TEST_RADII EQUAL_RADIUS 0 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"TEST_LINEAR PLANE:TOP PLANE:RIGHT CIRCLE 10 10 5"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"TEST_LINEAR CIRCLE 40 20 7"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"TEST_LINEAR DISTANCE 0:CENTER 1:CENTER 45"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_INFO",L"TEST_SYMMETRY"};CASE(t);}
    swprintf_s(cmd,1024,L"CREO_OP SKETCH_CONSTRAINT %ls TEST_LINE VERTICAL 0",name);reject(cmd,model);
    swprintf_s(cmd,1024,L"CREO_OP DATUM_PLANE %ls TEST_PLANE PLANE:TOP 10",name);reject(cmd,model);
    swprintf_s(cmd,1024,L"CREO_OP SKETCH_ADD %ls TEST_LINE LINE 0 0 0 0",name);reject(cmd,model);
    {const wchar_t *t[]={L"REFERENCES",L"TOP"};CASE(t);}
    {const wchar_t *t[]={L"DATUM_POINT",L"STALE_POINT CSYS:PRT_CSYS 2 4 6"};CASE(t);}
    swprintf_s(cmd,1024,L"CREO_OP REFERENCES %ls SELECTED",name);reject(cmd,model);
    {const wchar_t *t[]={L"SKETCH_BEGIN",L"DRAFT_SKETCH PLANE:TOP PLANE:RIGHT"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"DRAFT_SKETCH RECT 0 0 20 30"};CASE(t);}
    {ModelPlan cylinder={1,40,40,20,0,1};int surface=-1;
        e=model_create(&cylinder,name,&stage);trace("create private solid cylinder",e);if(e){failures++;goto done;}
        e=ProMdlnameInit(name,PRO_MDL_PART,&model);if(e){failures++;goto done;}
        e=ProSolidFeatVisit((ProSolid)model,find_cylinder_feature,NULL,&surface);if(e||surface<0){failures++;goto done;}
        swprintf_s(cmd,1024,L"CREO_OP DATUM_AXIS %ls CYLINDER_AXIS CYLINDER SURFACE:%d",name,surface);run(cmd,model);
        {const wchar_t *t[]={L"DATUM_PLANE",L"SOLID_PLANE PLANE:TOP 12"};CASE(t);}
        {const wchar_t *t[]={L"SKETCH_CREATE",L"SOLID_SKETCH PLANE:TOP PLANE:RIGHT RECT -10 -8 20 16"};CASE(t);}
        {const wchar_t *t[]={L"REFERENCES",L"AXIS:CYLINDER_AXIS"};CASE(t);}
    }
done:trace("FOUNDATION_SELFTEST_FAILURES",failures+(e?1:0));trace("FOUNDATION_SELFTEST_DONE",0);ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
