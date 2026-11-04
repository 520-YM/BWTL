#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
#include <ProUICmd.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int e){FILE *f=fopen("v21-native-result.log","a");if(f){fprintf(f,"%s: %d\n",label,e);fclose(f);}}
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
    f=_wfopen(L"v21-native-cases.log",L"a, ccs=UTF-8");if(f){fwprintf(f,L"%ls\nRESULT=%d\n%ls\n",protocol,e,message);fclose(f);}
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
static ProError pick_edge(ProGeomitem *item,ProError status,ProAppData data){ProEdge edge;double length;if(status)return status;if(*(int *)data>=0)return 0;if(!ProGeomitemToEdge(item,&edge)&&!ProEdgeLengthEval(edge,&length)&&length>20)*(int *)data=item->id;return 0;}
static ProError pick_edge_feature(ProFeature *f,ProError status,ProAppData data){ProError e;if(status)return status;e=ProFeatureGeomitemVisit(f,PRO_EDGE,pick_edge,NULL,data);return e==PRO_TK_E_NOT_FOUND?0:e;}
static ProError projected_length(ProGeomitem *item,ProError status,ProAppData data){ProCurve curve;ProEnttype type;double len;ProError e;if(status)return status;if((e=ProGeomitemToCurve(item,&curve))||(e=ProCurveTypeGet(curve,&type)))return e;if(type==PRO_ENT_CMP_CRV)return 0;e=ProCurveLengthEval(curve,&len);if(e)return e;*(double *)data+=len;return 0;}
static double sketch_line_length(ProMdl model,const wchar_t *name){ProModelitem feature;double len=0;if(ProModelitemByNameInit(model,PRO_FEATURE,(wchar_t *)name,&feature)||ProFeatureGeomitemVisit((ProFeature *)&feature,PRO_CURVE,projected_length,NULL,&len))return -1;return len;}
static void verify_projection_driving(ProMdl model){
    ProModelitem item;ProElement tree=NULL,element=NULL;ProElempath path=NULL;ProElempathItem part;ProFeatureCreateOptions *opts=NULL;ProErrorlist errors={0};ProError e;double before=sketch_line_length(model,L"PROJECT_SK"),after;
    if((e=ProModelitemByNameInit(model,PRO_FEATURE,L"PROJECT_SOURCE",&item))||(e=ProFeatureElemtreeExtract((ProFeature *)&item,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree))||(e=ProElempathAlloc(&path)))goto done;
    part.type=PRO_ELEM_PATH_ITEM_TYPE_ID;part.path_item.elem_id=PRO_E_CRV_ENTER_EQUATION;
    if((e=ProElempathDataSet(path,&part,1))||(e=ProElemtreeElementGet(tree,path,&element))||(e=ProElementWstringSet(element,L"x=40*t\ny=0\nz=20*t"))||(e=ProArrayAlloc(1,sizeof(ProFeatureCreateOptions),1,(ProArray *)&opts)))goto done;
    opts[0]=PRO_FEAT_CR_NO_OPTS;e=ProFeatureWithoptionsRedefine(NULL,(ProFeature *)&item,tree,opts,PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL,&errors);if(!e)e=part_regenerate(model);if(e)goto done;
    after=sketch_line_length(model,L"PROJECT_SK");trace("projection before length*1000",(int)lround(before*1000));trace("projection after length*1000",(int)lround(after*1000));trace("source after length*1000",(int)lround(sketch_line_length(model,L"PROJECT_SOURCE")*1000));if(before<=0||!part_close(after-before,sqrt(500.0)))e=PRO_TK_GENERAL_ERROR;
 done:trace("source curve edit drives projected geometry length",e);if(e)failures++;if(path)ProElempathFree(&path);if(tree)ProFeatureElemtreeFree((ProFeature *)&item,tree);if(opts)ProArrayFree((ProArray *)&opts);
}
static void reject_restored(const wchar_t *protocol,ProMdl model){
    FdTxn before={0},after={0};ProMassProperty a,b;int prior=failures,i;ProError e;
    if(ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&before)||ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&a)){failures++;return;}
    e=run(protocol,model);if(e)failures=prior;else failures++;
    if(ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&after)||before.before_count!=after.before_count||part_health(model)||ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&b)||!part_close(a.volume,b.volume))failures++;
    for(i=0;i<after.before_count;i++)if(!fd_was_feature(&before,after.before_ids[i]))failures++;
    trace("expected rejection preserves features, health and volume",failures==prior?0:-1);
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProMdl model=NULL;ProMdlName name;const char *stage="new template";ProError e;wchar_t cmd[1024];char marker[16];
    trace("ENTER_INITIALIZE",0);
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    trace("FOUNDATION_SELFTEST_STARTED",0);model_background_mode=1;
    e=model_create_empty(1,name,&stage);trace(stage,e);if(e)goto done;
    e=ProMdlnameInit(name,PRO_MDL_PART,&model);trace("resolve test model",e);if(e)goto done;
#define CASE(tail) do{swprintf_s(cmd,1024,L"CREO_OP %ls %ls %ls",tail[0],name,tail[1]);run(cmd,model);}while(0)
    {char focus[16];if(GetEnvironmentVariableA("BWTL_SOLID_FOCUS",focus,16)&&!strcmp(focus,"move")){
        {const wchar_t *t[]={L"SKETCH_CREATE",L"MOVE_SK PLANE:TOP PLANE:RIGHT CIRCLE 10 20 2"};CASE(t);}
        {const wchar_t *t[]={L"SKETCH_MOVE",L"MOVE_SK 0 3 4"};CASE(t);}
        {const wchar_t *t[]={L"SKETCH_INFO",L"MOVE_SK"};CASE(t);}
        goto done;
    }}
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
    e=model_create_empty(1,name,&stage);trace("new solid-operation test part",e);if(e){failures++;goto done;}e=ProMdlnameInit(name,PRO_MDL_PART,&model);if(e){failures++;goto done;}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"BASE_SK PLANE:TOP PLANE:RIGHT RECT -40 -30 80 60"};CASE(t);}
    {const wchar_t *t[]={L"SOLID_EXTRUDE",L"BASE_ADD BASE_SK ADD BLIND 10 POS"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,48000)){failures++;trace("base volume expected 48000",-1);}else trace("base volume expected 48000",0);}
    {const wchar_t *t[]={L"SOLID_EXTRUDE",L"APPEND_ADD BASE_SK ADD BLIND 20 POS"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,96000)){failures++;trace("append volume expected 96000",-1);}else trace("append volume expected 96000",0);}
    {int edge=-1;ProSolidFeatVisit((ProSolid)model,pick_edge_feature,NULL,&edge);
        {const wchar_t *t[]={L"SKETCH_CREATE",L"EDGE_PROJ PLANE:TOP PLANE:RIGHT CIRCLE 100 100 2"};CASE(t);}
        if(edge<0){failures++;trace("find usable real edge",-1);}else{swprintf_s(cmd,1024,L"CREO_OP SKETCH_PROJECT %ls EDGE_PROJ EDGE:%d",name,edge);run(cmd,model);}
    }
    {const wchar_t *t[]={L"SKETCH_CREATE",L"CUT_SK PLANE:TOP PLANE:RIGHT CIRCLE 0 0 5"};CASE(t);}
    {const wchar_t *t[]={L"SOLID_EXTRUDE",L"BLIND_CUT CUT_SK CUT BLIND 4 POS"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,96000-3.141592653589793*25*4)){failures++;trace("blind cut volume",-1);}else trace("blind cut volume",0);}
    {const wchar_t *t[]={L"SOLID_EXTRUDE",L"THROUGH_CUT CUT_SK CUT THROUGH 0 POS"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,96000-3.141592653589793*25*20)){failures++;trace("through cut volume",-1);}else trace("through cut volume",0);}
    {const wchar_t *t[]={L"SKETCH_DIM",L"BASE_SK LENGTH 0 NONE 100"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,120000-3.141592653589793*25*20)){failures++;trace("associative sketch edit updates solid volume",-1);}else trace("associative sketch edit updates solid volume",0);}
    {const wchar_t *t[]={L"DATUM_CURVE",L"PROJECT_SOURCE CSYS:PRT_CSYS 0 0 0 20 0 10"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"PROJECT_SK PLANE:TOP PLANE:RIGHT CIRCLE 30 30 2"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_PROJECT",L"PROJECT_SK CURVE:PROJECT_SOURCE"};CASE(t);}
    verify_projection_driving(model);
    {const wchar_t *t[]={L"SKETCH_CREATE",L"MOVE_SK PLANE:TOP PLANE:RIGHT CIRCLE 10 20 2"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_MOVE",L"MOVE_SK 0 3 4"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_ADD",L"MOVE_SK LINE 0 0 20 0"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_REMOVE",L"MOVE_SK ENTITY 1"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_INFO",L"MOVE_SK"};CASE(t);}
    {FdSketch edit={0};int *dims=NULL,n=0;if(!fd_sketch_open(model,L"MOVE_SK",&edit,0)&&!ProSecdimIdsGet(edit.section,&dims,&n)&&n){int dimension=dims[0];ProArrayFree((ProArray *)&dims);fd_sketch_free(&edit);swprintf_s(cmd,1024,L"CREO_OP SKETCH_REMOVE %ls MOVE_SK DIMENSION %d",name,dimension);run(cmd,model);}else{if(dims)ProArrayFree((ProArray *)&dims);fd_sketch_free(&edit);failures++;}}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"LINE_MOVE PLANE:FRONT PLANE:RIGHT LINE 1 2 11 2"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_MOVE",L"LINE_MOVE 0 3 -4"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"ARC_MOVE PLANE:TOP PLANE:RIGHT ARC 2 3 5 0 120"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_MOVE",L"ARC_MOVE 0 1 2"};CASE(t);}
    {FdSketch edit={0};int *constraints=NULL,n=0;if(!fd_sketch_open(model,L"LINE_MOVE",&edit,0)&&!ProSectionConstraintsIdsGet(edit.section,&constraints,&n)&&n){int constraint=constraints[0];ProArrayFree((ProArray *)&constraints);fd_sketch_free(&edit);swprintf_s(cmd,1024,L"CREO_OP SKETCH_REMOVE %ls LINE_MOVE CONSTRAINT %d",name,constraint);run(cmd,model);}else{if(constraints)ProArrayFree((ProArray *)&constraints);fd_sketch_free(&edit);failures++;}}
    swprintf_s(cmd,1024,L"CREO_OP SOLID_EXTRUDE %ls OPEN_ADD LINE_MOVE ADD BLIND 10 POS",name);reject_restored(cmd,model);
    {const wchar_t *t[]={L"SKETCH_CREATE",L"FAR_SK PLANE:TOP PLANE:RIGHT CIRCLE 200 200 2"};CASE(t);}
    swprintf_s(cmd,1024,L"CREO_OP SOLID_EXTRUDE %ls NO_HIT FAR_SK CUT THROUGH 0 POS",name);reject_restored(cmd,model);
    swprintf_s(cmd,1024,L"CREO_OP SKETCH_REMOVE %ls MOVE_SK ENTITY 0",name);reject_restored(cmd,model);
    e=model_create_empty(1,name,&stage);if(e){failures++;goto done;}e=ProMdlnameInit(name,PRO_MDL_PART,&model);if(e){failures++;goto done;}
    {const wchar_t *t[]={L"DATUM_AXIS",L"REV_AXIS PLANES PLANE:TOP PLANE:RIGHT"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"REV_SK PLANE:TOP PLANE:RIGHT RECT 10 0 10 30"};CASE(t);}
    {const wchar_t *t[]={L"SOLID_REVOLVE",L"REV_ADD REV_SK AXIS:REV_AXIS 360 POS"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,9000*3.141592653589793)){failures++;trace("revolved solid volume expected 9000*pi",-1);}else trace("revolved solid volume expected 9000*pi",0);}
    e=model_create_empty(1,name,&stage);if(e){failures++;goto done;}e=ProMdlnameInit(name,PRO_MDL_PART,&model);if(e){failures++;goto done;}
    {const wchar_t *t[]={L"DATUM_AXIS",L"REV_AXIS PLANES PLANE:TOP PLANE:RIGHT"};CASE(t);}
    {const wchar_t *t[]={L"SKETCH_CREATE",L"REV_SK PLANE:TOP PLANE:RIGHT RECT 10 0 10 30"};CASE(t);}
    {const wchar_t *t[]={L"SOLID_REVOLVE",L"QUARTER_REV REV_SK AXIS:REV_AXIS 90 NEG"};CASE(t);}
    {ProMassProperty mp;ProError v=ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mp);if(v||!part_close(mp.volume,2250*3.141592653589793)){failures++;trace("quarter negative revolve volume",-1);}else trace("quarter negative revolve volume",0);}
done:trace("FOUNDATION_SELFTEST_FAILURES",failures+(e?1:0));trace("FOUNDATION_SELFTEST_DONE",0);ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
