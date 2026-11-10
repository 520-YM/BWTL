#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
typedef struct {ProMdl model;int face,edge,bend;double z;} ShFixture;
static ProError sh_test_edge(ProEdge edge,ProError status,ProAppData data);
static ProError sh_test_contour(ProContour contour,ProError status,ProAppData data){
    ShFixture *s=data;ProSurface surface;if(status)return status;
    if(ProSurfaceInit((ProSolid)s->model,s->face,&surface))return PRO_TK_BAD_INPUTS;
    return ProContourEdgeVisit(surface,contour,sh_test_edge,NULL,s);
}
static ProError sh_test_face(ProSurface surface,ProError status,ProAppData data){
    ShFixture *s=data;ProSmtSurfType side;ProGeomitem item;ProSrftype type;ProVector p,n;
    if(status)return status;
    if(ProSmtSurfaceTypeGet((ProPart)s->model,surface,&side)||side!=PRO_SMT_SURF_FACE||ProSurfaceToGeomitem((ProSolid)s->model,surface,&item))return 0;
    if(!fd_plane_data(&item,p,n)&&fabs(n[2])>.99){s->face=item.id;s->z=p[2];{FILE *l=fopen("sheetmetal-geometry.log","a");if(l){fprintf(l,"face %d p=%g,%g,%g n=%g,%g,%g\n",item.id,p[0],p[1],p[2],n[0],n[1],n[2]);fclose(l);}}}
    if(!ProSurfaceTypeGet(surface,&type)&&type==PRO_SRF_CYL)s->bend=item.id;return 0;
}
static ProError sh_test_edge(ProEdge edge,ProError status,ProAppData data){
    ShFixture *s=data;ProSurface a,b;ProSmtSurfType at,bt;Pro3dPnt p,q;ProVector d,e,f;ProEnttype type;int id;
    if(status)return status;
    if(ProEdgeTypeGet(edge,&type)||type!=PRO_ENT_LINE||ProEdgeNeighborsGet(edge,NULL,NULL,&a,&b)||
       ProSmtSurfaceTypeGet((ProPart)s->model,a,&at)||ProSmtSurfaceTypeGet((ProPart)s->model,b,&bt))return 0;
    if(!((at==PRO_SMT_SURF_FACE&&bt==PRO_SMT_SURF_SIDE)||(bt==PRO_SMT_SURF_FACE&&at==PRO_SMT_SURF_SIDE)))return 0;
    if(ProEdgeXyzdataEval(edge,0,p,d,e,f)||ProEdgeXyzdataEval(edge,1,q,d,e,f))return 0;
    if(fabs(p[1])<.001&&fabs(q[1])<.001&&fabs(q[0]-p[0])>50){ProEdgeIdGet(edge,&id);s->edge=id;}return 0;
}
static void sh_test_refs(ProMdl model,ShFixture *s){ProSurface surface;memset(s,0,sizeof(*s));s->model=model;ProSolidSurfaceVisit((ProSolid)model,sh_test_face,NULL,s);if(!ProSurfaceInit((ProSolid)model,s->face,&surface))ProSurfaceContourVisit(surface,sh_test_contour,NULL,s);trace("sheetmetal driving face",s->face);trace("sheetmetal attachment edge",s->edge);trace("sheetmetal bend surface",s->bend);}
static ProError sh_test_run(ProMdl model,const wchar_t *text,int reject){
    int oldfail=failures;double oldvolume=volume(model);ProError e=run(model,text,reject);
    if(failures!=oldfail){FILE *f=_wfopen(L"sheetmetal-failures.log",L"a, ccs=UTF-8");if(f){fwprintf(f,L"%ls\nreturned=%d reject=%d failures=%d volume=%.15g -> %.15g guard=%d\n",text,e,reject,failures-oldfail,oldvolume,volume(model),sh_recovery_uncertain);fclose(f);}}return e;
}
static int sh_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl model=NULL;ProMdlName name;ProError e;wchar_t text[1400];ShFixture f;const char *stage="sheetmetal fixture";
    model_background_mode=0;
#define CASE(reject,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),sh_test_run(model,text,reject))
#define EMPTY() do{if((e=model_create_empty(2,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}}while(0)
#define PLATE() do{EMPTY();CASE(0,L"SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);if(CASE(0,L"SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name))goto done;sh_test_refs(model,&f);}while(0)
    PLATE();expect_volume(model,9600);
    CASE(0,L"SMT_INFO %ls",name);
    CASE(1,L"SMT_FIRST_WALL %ls SECOND SKETCH:OUTLINE 2 POS",name);
    CASE(1,L"SMT_FLANGE %ls BAD_EDGE EDGE:999999 20 90 2 POS",name);
    CASE(0,L"SMT_FLANGE %ls FLANGE EDGE:%d 20 90 2 POS",name,f.edge);
    sh_test_refs(model,&f);
    CASE(0,L"SMT_UNBEND %ls FLAT SURFACE:%d",name,f.face);
    CASE(0,L"SMT_REBEND %ls BACK SURFACE:%d",name,f.face);
    EMPTY();CASE(0,L"SMT_SKETCH %ls OUTLINE2 PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);
    CASE(0,L"SMT_FIRST_WALL %ls BASE2 SKETCH:OUTLINE2 2 POS",name);
    sh_test_refs(model,&f);
    CASE(0,L"SMT_SKETCH %ls BEND_LINE PLANE:FRONT PLANE:RIGHT LINE 0 40 80 40",name);
    CASE(0,L"SMT_BEND %ls BEND_A CURVE:BEND_LINE 90 2 POS FIX1",name);
    sh_test_refs(model,&f);
    CASE(0,L"SMT_ALLOWANCE %ls BEND_A K 0.4",name);
    CASE(0,L"SMT_ALLOWANCE %ls BEND_A BD 3",name);
    CASE(0,L"SMT_ALLOWANCE %ls BEND_A TABLE table1",name);
    CASE(1,L"SMT_ALLOWANCE %ls BEND_A TABLE BWTL_MISSING_TABLE",name);
    CASE(0,L"SMT_ALLOWANCE %ls BEND_A K 0.4",name);
    CASE(0,L"SMT_RELIEF %ls BEND_A ALL RECT 3 4 3",name);
    CASE(0,L"SMT_RELIEF %ls BEND_A ALL OBROUND 3 4 3",name);
    CASE(1,L"SMT_RELIEF %ls ABSENT ALL RECT 3 4 3",name);
    CASE(0,L"SMT_DXF %ls BENT_BLANK SURFACE:%d",name,f.face);
    CASE(1,L"SMT_DXF %ls BENT_BLANK SURFACE:%d",name,f.face);
    CASE(0,L"SMT_UNBEND %ls UNBEND_A SURFACE:%d",name,f.face);
    CASE(0,L"SMT_SKETCH %ls HOLE_LINE PLANE:FRONT PLANE:RIGHT CIRCLE 20 10 3",name);
    CASE(0,L"SMT_CUT %ls HOLE_A SKETCH:HOLE_LINE NEG",name);
    CASE(0,L"SMT_REBEND %ls BEND_BACK SURFACE:%d",name,f.face);
    CASE(0,L"SMT_DXF %ls HOLED_BLANK SURFACE:%d",name,f.face);
    PLATE();
    CASE(1,L"SMT_HEM %ls OUTSIDE EDGE:%d OPEN 200 2 POS",name,f.edge);
    CASE(1,L"SMT_HEM %ls OVER_CURL EDGE:%d ROUND 30 2 POS",name,f.edge);
    CASE(0,L"SMT_HEM %ls OPEN_HEM EDGE:%d OPEN 20 2 POS",name,f.edge);
    PLATE();
    CASE(0,L"SMT_SKETCH %ls BEAD_SK SURFACE:%d PLANE:RIGHT RECT 20 -32 20 12",name,f.face);
    CASE(0,L"SMT_FORM %ls BEAD SKETCH:BEAD_SK 8 1 POS",name);
    CASE(1,L"SMT_DXF %ls FORMED_REJECT SURFACE:%d",name,f.face);
    PLATE();
    CASE(0,L"SMT_HEM %ls CURL EDGE:%d ROUND 10 4 POS",name,f.edge);
    PLATE();
    CASE(0,L"SMT_SKETCH %ls ROUND_SK SURFACE:%d PLANE:RIGHT CIRCLE 30 -30 8",name,f.face);
    CASE(1,L"SMT_BEND %ls CIRCLE_REJECT CURVE:ROUND_SK 90 2 POS FIX1",name);
    CASE(0,L"SMT_FORM %ls ROUND_FORM SKETCH:ROUND_SK 5 1 POS",name);
    EMPTY();
    CASE(0,L"SMT_SKETCH %ls SQUARE PLANE:FRONT PLANE:RIGHT RECT 0 0 40 40",name);
    CASE(0,L"SMT_FIRST_WALL %ls SQUARE_BASE SKETCH:SQUARE 2 NEG",name);expect_volume(model,3200);
    sh_test_refs(model,&f);CASE(0,L"SMT_DXF %ls SQUARE_BLANK SURFACE:%d",name,f.face);
    PLATE();
    CASE(0,L"SMT_FLANGE %ls ANGLE_FLANGE EDGE:%d 15 45 3 NEG",name,f.edge);
    {ModelPlan plan={0,80,60,2,0,1};const char *stage="conversion fixture";
     if((e=model_create(&plan,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&model))){trace(stage,e);failures++;goto done;}
     CASE(1,L"SMT_CONVERT %ls WRONG_FACE SURFACE:999999 2",name);
     CASE(1,L"SMT_CONVERT %ls WRONG_THICK SURFACE:96 3",name);
     CASE(0,L"SMT_CONVERT %ls CONVERT SURFACE:96 2",name);
     CASE(1,L"SMT_CONVERT %ls ALREADY_SMT SURFACE:96 2",name);}
done:trace("SHEETMETAL_CASES",cases);trace("SHEETMETAL_FAILURES",failures);trace("SHEETMETAL_DONE",1);ProEngineerEnd();return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;char marker[16];ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    {HMODULE modules[2048];DWORD bytes;if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
     for(int i=0;i<(int)(bytes/sizeof(HMODULE));i++){wchar_t path[MAX_PATH];GetModuleFileNameW(modules[i],path,MAX_PATH);CharLowerBuffW(path,(DWORD)wcslen(path));if(wcsstr(path,L"creo_safe_resident"))return PRO_TK_BAD_CONTEXT;}trace("legacy_bridge_loaded",0);}
    if((e=ProCmdActionAdd("BWTLSheetmetalProbe",sh_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;
    return ProMacroLoad(L"~ Command `BWTLSheetmetalProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
