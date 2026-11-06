#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <psapi.h>
#include <ProToolkit.h>
#include <ProCore.h>
#include <ProUtil.h>
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){return _wfopen(name,mode);}
static void trace(const char *label,int e){FILE *f=fopen("machining-result.log","a");if(f){fprintf(f,"%s: %d\n",label,e);fclose(f);}}
#include "../src/native_model.h"
#include "../src/part_tools.h"
static int failures=0,cases=0;
typedef struct {int top,bottom,x,y,xfar,yfar,vertical[4],nv;} Fixture;
static ProError pick(ProGeomitem *item,ProError status,ProAppData data){
    Fixture *f=data;ProVector p,n,a,b,v;ProEdge edge;ProError e;if(status)return status;
    if(item->type==PRO_SURFACE){
        if(fd_plane_data(item,p,n))return 0;
        ProFeature owner;ProFeattype ft=PRO_FEAT_DATUM;ProGeomitemFeatureGet(item,&owner);ProFeatureTypeGet(&owner,&ft);
        if(ft!=PRO_FEAT_DATUM&&fabs(n[2])>0.99){if(fabs(p[2]-20)<1e-6)f->top=item->id;if(fabs(p[2])<1e-6)f->bottom=item->id;}
        if(!f->x&&n[0]>0.99&&fabs(p[0])<1e-6)f->x=item->id;
        if(!f->y&&n[1]>0.99&&fabs(p[1])<1e-6)f->y=item->id;
        if(n[0]>0.99&&fabs(p[0]-80)<1e-6)f->xfar=item->id;
        if(n[1]>0.99&&fabs(p[1]-60)<1e-6)f->yfar=item->id;
    }else{
        if((e=ProGeomitemToEdge(item,&edge))||(e=ProEdgeXyzdataEval(edge,0,p,a,b,v))||(e=ProEdgeXyzdataEval(edge,1,n,a,b,v)))return e;
        if(fabs(p[0]-n[0])<1e-6&&fabs(p[1]-n[1])<1e-6&&fabs(fabs(p[2]-n[2])-20)<1e-6&&f->nv<4)f->vertical[f->nv++]=item->id;
    }return 0;
}
static ProError pick_feature(ProFeature *f,ProError status,ProAppData data){ProError e;if(status)return status;e=ProFeatureGeomitemVisit(f,PRO_SURFACE,pick,NULL,data);if(e&&e!=PRO_TK_E_NOT_FOUND)return e;e=ProFeatureGeomitemVisit(f,PRO_EDGE,pick,NULL,data);return e==PRO_TK_E_NOT_FOUND?0:e;}
typedef struct {ProMdl model;Fixture *f;} PickFinal;
static ProError pick_final_surface(ProSurface surface,ProError status,ProAppData data){PickFinal *p=data;ProGeomitem item;ProError e;if(status)return status;if((e=ProSurfaceToGeomitem((ProSolid)p->model,surface,&item)))return e;return pick(&item,0,p->f);}
static ProError fixture(ProMdl *model,ProMdlName name,Fixture *f){
    ModelPlan p={0,80,60,20,0,1};const char *stage="new fixture";ProError e;memset(f,0,sizeof(*f));
    if((e=model_create(&p,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,model)))return e;
    if((e=part_regenerate(*model)))return e;
    e=ProSolidFeatVisit((ProSolid)*model,pick_feature,NULL,f);if(e)return e;
    {PickFinal final={*model,f};f->top=0;f->bottom=0;if((e=ProSolidSurfaceVisit((ProSolid)*model,pick_final_surface,NULL,&final)))return e;}
    return f->top&&f->bottom&&f->x&&f->y&&f->nv==4?0:PRO_TK_E_NOT_FOUND;
}
static double volume(ProMdl model){ProMassProperty m;if(ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&m))return -1;return m.volume;}
static ProError run(ProMdl model,const wchar_t *protocol,int reject){
    PartCommand cmd={0};wchar_t message[PART_TEXT_MAX]=L"";FdTxn before={0},after={0};double old=volume(model);int i;ProError e;
    ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&before);
    if(part_command_parse(protocol,&cmd)!=1)e=PRO_TK_BAD_INPUTS;else e=foundation_execute(&cmd,model,message,PART_TEXT_MAX);
    if(!e&&!reject&&cmd.verb==PART_FOUNDATION&&wcscmp(cmd.arg[0],L"MACHINING_INFO")){
        ProModelitem item;ProElement tree=NULL;ProPath path;
        if(!ProModelitemByNameInit(model,PRO_FEATURE,cmd.arg[2],&item)&&!ProFeatureElemtreeExtract((ProFeature *)&item,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)){
            swprintf_s(path,PRO_PATH_SIZE,L"%ls.xml",cmd.arg[2]);ProElemtreeWrite(tree,PRO_ELEMTREE_XML,path);ProFeatureElemtreeFree((ProFeature *)&item,tree);
        }
    }
    cases++;if((reject&&!e)||(!reject&&e))failures++;
    if(reject){
        ProSolidFeatVisit((ProSolid)model,fd_feature_ids,NULL,&after);
        if(before.before_count!=after.before_count||part_health(model)||!part_close(old,volume(model)))failures++;
        for(i=0;i<after.before_count;i++)if(!fd_was_feature(&before,after.before_ids[i]))failures++;
    }
    {FILE *log=_wfopen(L"machining-cases.log",L"a, ccs=UTF-8");if(log){fwprintf(log,L"%ls\nRESULT=%d EXPECT_REJECT=%d\n%ls\n",protocol,e,reject,message);fclose(log);}}
    return e;
}
static void expect_volume(ProMdl model,double expected){double actual=volume(model);if(fabs(actual-expected)>fmax(1e-5,expected*1e-7))failures++;trace("volume verification",fabs(actual-expected)<=fmax(1e-5,expected*1e-7)?0:-1);}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProMdl model=NULL;ProMdlName name;Fixture f;ProError e=0;wchar_t cmd[1024],refs[28000];int i;char marker[16];
    HMODULE modules[2048];DWORD bytes;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
    for(i=0;i<(int)(bytes/sizeof(HMODULE));i++){wchar_t path[MAX_PATH];GetModuleFileNameW(modules[i],path,MAX_PATH);CharLowerBuffW(path,(DWORD)wcslen(path));if(wcsstr(path,L"creo_safe_resident"))return PRO_TK_BAD_CONTEXT;}
    trace("legacy_bridge_loaded",0);model_background_mode=1;
#define NEW() do{if((e=fixture(&model,name,&f))){trace("fixture failure",e);failures++;goto done;}}while(0)
#define HOLE(label,face,xx,yy,mode,depth,reject) do{swprintf_s(cmd,1024,L"CREO_OP HOLE %ls %ls SURFACE:%d SURFACE:%d %g SURFACE:%d %g 10 %ls %g",name,label,face,f.x,(double)xx,f.y,(double)yy,mode,(double)depth);run(model,cmd,reject);}while(0)
    NEW();
    if(mc_references(model,refs,28000))failures++;
    {FILE *log=_wfopen(L"machining-refs.txt",L"wt, ccs=UTF-8");if(log){fwprintf(log,L"%ls",refs);fclose(log);}}
    HOLE(L"THRU",f.top,20,20,L"THROUGH",0,0);expect_volume(model,96000-500*3.141592653589793);
    HOLE(L"THRU",f.top,40,40,L"THROUGH",0,1);
    HOLE(L"NO_HIT",f.top,200,200,L"THROUGH",0,1);
    NEW();HOLE(L"BLIND",f.top,20,20,L"BLIND",5,0);expect_volume(model,96000-125*3.141592653589793);
    HOLE(L"OVERLAP",f.top,20,20,L"BLIND",5,1);
    HOLE(L"BREAK_OUT",f.top,40,40,L"BLIND",25,1);
    HOLE(L"SIDE_BREAK",f.top,1,30,L"BLIND",5,1);
    NEW();HOLE(L"BOTTOM",f.bottom,20,20,L"BLIND",5,0);expect_volume(model,96000-125*3.141592653589793);
    NEW();f.x=f.xfar;f.y=f.yfar;HOLE(L"NEGATIVE",f.top,-60,-40,L"BLIND",5,0);expect_volume(model,96000-125*3.141592653589793);
    NEW();
    swprintf_s(cmd,1024,L"CREO_OP DATUM_PLANE %ls X20 SURFACE:%d 20",name,f.x);run(model,cmd,0);
    swprintf_s(cmd,1024,L"CREO_OP DATUM_PLANE %ls Y20 SURFACE:%d 20",name,f.y);run(model,cmd,0);
    {FdRef x={0},y={0};if(fd_ref_resolve(model,L"PLANE:X20",PRO_SURFACE,&x)||fd_ref_resolve(model,L"PLANE:Y20",PRO_SURFACE,&y)){failures++;goto done;}f.x=x.item.id;f.y=y.item.id;fd_ref_free(&x);fd_ref_free(&y);}
    HOLE(L"ALIGNED",f.top,0,0,L"BLIND",5,0);expect_volume(model,96000-125*3.141592653589793);
    NEW();
    swprintf_s(cmd,1024,L"CREO_OP EDGE_ROUND %ls R2 2 EDGE:%d",name,f.vertical[0]);run(model,cmd,0);
    expect_volume(model,96000-(4-3.141592653589793)*4*20/4);
    NEW();swprintf_s(cmd,1024,L"CREO_OP EDGE_CHAMFER %ls C2 2 EDGE:%d",name,f.vertical[0]);run(model,cmd,0);expect_volume(model,95960);
    NEW();swprintf_s(cmd,1024,L"CREO_OP EDGE_ROUND %ls FOUR 1 EDGE:%d EDGE:%d EDGE:%d EDGE:%d",name,f.vertical[0],f.vertical[1],f.vertical[2],f.vertical[3]);run(model,cmd,0);expect_volume(model,96000-4*(1-3.141592653589793/4)*20);
    NEW();swprintf_s(cmd,1024,L"CREO_OP EDGE_CHAMFER %ls FOUR 1 EDGE:%d EDGE:%d EDGE:%d EDGE:%d",name,f.vertical[0],f.vertical[1],f.vertical[2],f.vertical[3]);run(model,cmd,0);expect_volume(model,95960);
    NEW();swprintf_s(cmd,1024,L"CREO_OP EDGE_ROUND %ls HUGE 1000 EDGE:%d",name,f.vertical[0]);run(model,cmd,1);
    swprintf_s(cmd,1024,L"CREO_OP EDGE_CHAMFER %ls HUGE 1000 EDGE:%d",name,f.vertical[0]);run(model,cmd,1);
    swprintf_s(cmd,1024,L"CREO_OP EDGE_ROUND %ls STALE 1 EDGE:999999",name);run(model,cmd,1);
done:trace("MACHINING_CASES",cases);trace("MACHINING_FAILURES",failures);trace("MACHINING_DONE",1);ProEngineerEnd();return 0;
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
