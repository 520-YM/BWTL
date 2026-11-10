#define BWTL_REUSE_MACHINING_FIXTURE
#define BWTL_REUSE_DIAGNOSTIC_TEST
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
static ProError ux_test_run(ProMdl model,const wchar_t *text,int reject){int old=failures;ProError e=run(model,text,reject);if(failures!=old){FILE *f=_wfopen(L"reuse-failures.log",L"a, ccs=UTF-8");if(f){fwprintf(f,L"%ls\nreturned=%d reject=%d failures=%d guard=%d\n",text,e,reject,failures-old,ux_recovery_uncertain);fclose(f);}}return e;}
static void ux_check(int pass,const char *label){trace(label,pass?0:-1);cases++;if(!pass)failures++;}
typedef struct {double xy[128][2];int count;} UxHoleSet;
static ProError ux_test_cylinder(ProSurface surface,ProError status,ProAppData data){UxHoleSet *set=data;ProSrftype type;ProGeomitemdata *raw=NULL;ProError e;if(status)return status;if(ProSurfaceTypeGet(surface,&type)||type!=PRO_SRF_CYL)return 0;if((e=ProSurfaceDataGet(surface,&raw)))return e;ProCylinderdata *c=&raw->data.p_surface_data->srf_shape.cylinder;
 if(fabs(c->e3[2])>.999&&part_close(c->radius,2)){int found=0;for(int i=0;i<set->count;i++)if(part_close(set->xy[i][0],c->origin[0])&&part_close(set->xy[i][1],c->origin[1]))found=1;if(!found&&set->count<128){set->xy[set->count][0]=c->origin[0];set->xy[set->count++][1]=c->origin[1];}}
 ProGeomitemdataFree(&raw);return 0;}
static void ux_centers(ProMdl model,const double points[][2],int n,const char *label){UxHoleSet actual={0};int pass=!ProSolidSurfaceVisit((ProSolid)model,ux_test_cylinder,NULL,&actual)&&actual.count==n;for(int i=0;i<n;i++){int found=0;for(int j=0;j<actual.count;j++)if(part_close(points[i][0],actual.xy[j][0])&&part_close(points[i][1],actual.xy[j][1]))found=1;if(!found)pass=0;}ux_check(pass,label);}
static void ux_update_hole(ProMdl model,const wchar_t *name,double expected){PartSnapshot *s=calloc(1,sizeof(*s));ProFeature feature;int pass=s&&!ux_named_feature(model,name,&feature);int found=-1;if(pass){ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,part_dimension_visit,NULL,s);for(int i=0;i<s->dimensions;i++)if(s->dimension[i].feature==feature.id&&s->dimension[i].type==PRODIMTYPE_DIAMETER&&part_close(s->dimension[i].value,4))found=i;pass=found>=0&&!ProDimensionValueSet(&s->dimension[found].handle,6)&&!part_regenerate(model)&&part_close(volume(model),expected);}ux_check(pass,"native source diameter propagates to dependent pattern or mirror");free(s);}
static ProError ux_boss_top(ProSurface surface,ProError status,ProAppData data){ProGeomitem item;ProVector p,n;if(status)return status;ProMdl model;ProMdlCurrentGet(&model);if(!ProSurfaceToGeomitem((ProSolid)model,surface,&item)&&!fd_plane_data(&item,p,n)&&fabs(n[2])>.99&&part_close(p[2],28))*(int *)data=item.id;return 0;}
static ProError ux_boss_edge(ProGeomitem *item,ProError status,ProAppData data){ProEdge edge;ProEnttype type;ProVector p,q,a,b,c;if(status)return status;if(ProGeomitemToEdge(item,&edge)||ProEdgeTypeGet(edge,&type)||type!=PRO_ENT_LINE||ProEdgeXyzdataEval(edge,0,p,a,b,c)||ProEdgeXyzdataEval(edge,1,q,a,b,c))return 0;if(part_close(p[0],15)&&part_close(q[0],15)&&part_close(p[1],24)&&part_close(q[1],24)&&part_close(fabs(p[2]-q[2]),8))*(int *)data=item->id;return 0;}
static void ux_family_disk_check(const wchar_t *symbol){ProMdl generic=NULL,instance=NULL;ProPath directory,path;ProFamtable table;ProFamtableItem column;ProFaminstance row;ProMdlName name;ProParamvalue value;ProError e=0;UxDimension dim;int pass=1;const double sizes[]={15,25,30};
 if(ProMdlnameInit(L"UX_FAMILY",PRO_MDL_PART,&generic)){ux_check(0,"family saved native mother exists");return;}
 for(int i=0;i<3;i++){swprintf_s(name,PRO_MDLNAME_SIZE,L"UX_FAMILY_%02d",i+1);if(!ProMdlnameInit(name,PRO_MDL_PART,&instance)&&ProMdlErase(instance))pass=0;instance=NULL;}
 if(ProMdlErase(generic)||ProDirectoryCurrentGet(directory))pass=0;generic=NULL;swprintf_s(path,PRO_PATH_SIZE,L"%ls\\UX_FAMILY_family\\UX_FAMILY.prt",directory);
 if(pass&&(e=ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&generic)))pass=0;
 if(pass&&(ProFamtableInit(generic,&table)||ProFamtableCheck(&table)||ux_dimension(generic,symbol,&dim)||ProModelitemToFamtableItem((ProModelitem *)&dim.dim,&column)))pass=0;
 for(int i=0;i<3&&pass;i++){swprintf_s(name,PRO_MDLNAME_SIZE,L"UX_FAMILY_%02d",i+1);if(ProFaminstanceInit(name,&table,&row)||ProFaminstanceValueGet(&row,&column,&value)||!part_close(value.value.d_val,sizes[i])||ProFaminstanceRetrieve(&row,&instance)||ux_dimension(instance,symbol,&dim)||!part_close(dim.value,sizes[i])||part_health(instance))pass=0;if(instance){ProMdlErase(instance);instance=NULL;}}
 if(generic)ProMdlErase(generic);trace("family disk reload error",e);ux_check(pass,"native family table and actual instances reopen from disk");}
static void ux_exclusion_disk(ProMdl source){ProMdl copy=NULL;ProPath folder,path;ProFeature header,leader,mask;ProPattern pattern;int pass=!ProMdlnameCopy(source,L"UX_MASK_RELOAD",&copy);if(pass){pass=!ProMdlErase(copy);copy=NULL;}if(pass){ProDirectoryCurrentGet(folder);swprintf_s(path,PRO_PATH_SIZE,L"%ls\\UX_MASK_RELOAD.prt",folder);pass=!ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&copy)&&!ux_skip_regen(copy)&&part_close(volume(copy),96000-5*80*AS_PI)&&!ux_named_feature(copy,L"GRID",&header)&&!ProFeaturePatternGet(&header,PRO_FEAT_PATTERN,&pattern)&&!ProPatternLeaderGet(&pattern,&leader);}ux_check(pass,"excluded geometry reopens from native disk copy and fully regenerates");if(pass){ux_test_run(copy,L"CREO_OP REUSE_SKIP UX_MASK_RELOAD GRID 2 RESTORE",0);expect_volume(copy,96000-6*80*AS_PI);}if(copy)ProMdlErase(copy);ux_copy_files_remove(L"UX_MASK_RELOAD");expect_volume(source,96000-5*80*AS_PI);}
static int ux_test_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
 ProMdl model=NULL;ProMdlName name;Fixture f;ProError e;wchar_t text[1600];model_background_mode=0;
#define NEW() do{if((e=fixture(&model,name,&f))){trace("reuse fixture",e);failures++;goto done;}ux_recovery_uncertain=0;}while(0)
#define CASE(reject,format,...) (swprintf_s(text,_countof(text),L"CREO_OP " format,__VA_ARGS__),ux_test_run(model,text,reject))
#define HOLE(label,xx,yy) CASE(0,L"HOLE %ls %ls SURFACE:%d SURFACE:%d %g SURFACE:%d %g 4 THROUGH 0",name,label,f.top,f.x,(double)xx,f.y,(double)yy)
 NEW();HOLE(L"SEED",10,10);CASE(1,L"REUSE_GRID %ls BAD_GRID SEED SURFACE:%d 3 20 POS SURFACE:%d 2 20 POS",name,f.x,f.x);CASE(0,L"REUSE_GRID %ls GRID SEED SURFACE:%d 3 20 POS SURFACE:%d 2 20 POS",name,f.x,f.y);expect_volume(model,96000-6*80*AS_PI);
 CASE(0,L"REUSE_SKIP %ls GRID 2 SKIP",name);expect_volume(model,96000-5*80*AS_PI);CASE(1,L"REUSE_SKIP %ls GRID 2 SKIP",name);
 {const double p[][2]={{10,10},{50,10},{10,30},{30,30},{50,30}};ux_centers(model,p,5,"excluded exact second geometry after full regeneration");}
 ux_exclusion_disk(model);
 CASE(0,L"REUSE_SKIP %ls GRID 6 SKIP",name);expect_volume(model,96000-4*80*AS_PI);
 {const double p[][2]={{10,10},{50,10},{10,30},{30,30}};ux_centers(model,p,4,"multiple exclusions preserve all other actual hole positions");}
 CASE(0,L"REUSE_SKIP %ls GRID 2 RESTORE",name);expect_volume(model,96000-5*80*AS_PI);CASE(0,L"REUSE_SKIP %ls GRID 6 RESTORE",name);CASE(1,L"REUSE_SKIP %ls GRID 6 RESTORE",name);expect_volume(model,96000-6*80*AS_PI);
 {const double p[][2]={{10,10},{30,10},{50,10},{10,30},{30,30},{50,30}};ux_centers(model,p,6,"grid measured hole centers");}
 CASE(1,L"REUSE_SKIP %ls GRID 1 SKIP",name);CASE(1,L"REUSE_SKIP %ls GRID 64 SKIP",name);CASE(0,L"REUSE_SKIP %ls GRID 6 SKIP",name);CASE(0,L"REUSE_INFO %ls",name);CASE(0,L"REUSE_SKIP %ls GRID 6 RESTORE",name);
 CASE(0,L"REUSE_SKIP %ls GRID 2 SKIP",name);ux_update_hole(model,L"SEED",96000-5*180*AS_PI);CASE(0,L"REUSE_SKIP %ls GRID 2 RESTORE",name);ux_check(part_close(volume(model),96000-6*180*AS_PI),"restored member follows changed source diameter");
 NEW();CASE(0,L"DATUM_PLANE %ls Z20 PLANE:FRONT 20",name);CASE(0,L"SKETCH_CREATE %ls PATH PLANE:Z20 PLANE:RIGHT LINE 10 10 70 10",name);HOLE(L"CURVE_SEED",10,10);CASE(0,L"REUSE_CURVE %ls CURVED CURVE_SEED CURVE:PATH COUNT 4 POS",name);expect_volume(model,96000-4*80*AS_PI);
 {const double p[][2]={{10,10},{30,10},{50,10},{70,10}};ux_centers(model,p,4,"curve count measured hole centers");}
 NEW();CASE(0,L"DATUM_PLANE %ls CZ PLANE:FRONT 20",name);CASE(0,L"SKETCH_CREATE %ls ARC_PATH PLANE:CZ PLANE:RIGHT ARC 40 30 20 0 180",name);HOLE(L"ARC_SEED",60,30);CASE(0,L"REUSE_CURVE %ls ARC_PATTERN ARC_SEED CURVE:ARC_PATH COUNT 3 POS",name);expect_volume(model,96000-3*80*AS_PI);
 {const double p[][2]={{60,30},{40,50},{20,30}};ux_centers(model,p,3,"arc pattern actual noncollinear centers");}
 NEW();CASE(0,L"DATUM_PLANE %ls SZ PLANE:FRONT 20",name);CASE(0,L"SKETCH_CREATE %ls SPACE_PATH PLANE:SZ PLANE:RIGHT LINE 10 10 70 10",name);HOLE(L"SPACED_SEED",10,10);CASE(0,L"REUSE_CURVE %ls SPACED SPACED_SEED CURVE:SPACE_PATH SPACING 15 POS",name);expect_volume(model,96000-5*80*AS_PI);
 {const double p[][2]={{10,10},{25,10},{40,10},{55,10},{70,10}};ux_centers(model,p,5,"curve spacing measured hole centers");}
 NEW();CASE(0,L"DATUM_PLANE %ls POINT_Z20 PLANE:FRONT 20",name);CASE(0,L"REUSE_POINT_SKETCH %ls LOCATIONS PLANE:POINT_Z20 PLANE:RIGHT 10 10 35 20 60 40",name);HOLE(L"POINT_SEED",10,10);CASE(0,L"REUSE_POINTS %ls POINT_PATTERN POINT_SEED SKETCH:LOCATIONS",name);expect_volume(model,96000-3*80*AS_PI);
 {const double p[][2]={{10,10},{35,20},{60,40}};ux_centers(model,p,3,"point pattern measured irregular centers");}
 NEW();HOLE(L"LATE_SEED",10,10);CASE(0,L"REUSE_POINT_SKETCH %ls LATE_POINTS PLANE:FRONT PLANE:RIGHT 10 10 30 20",name);CASE(1,L"REUSE_POINTS %ls REJECT_LATE LATE_SEED SKETCH:LATE_POINTS",name);
 NEW();HOLE(L"ONE",10,10);HOLE(L"TWO",20,20);CASE(0,L"DATUM_PLANE %ls X40 SURFACE:%d 40",name,f.x);CASE(0,L"REUSE_MIRROR %ls MIRRORED PLANE:X40 ONE TWO",name);expect_volume(model,96000-4*80*AS_PI);
 {const double p[][2]={{10,10},{20,20},{70,10},{60,20}};ux_centers(model,p,4,"combined mirror actual centers");}
 ux_update_hole(model,L"ONE",96000-(2*180+2*80)*AS_PI);
 NEW();CASE(0,L"DATUM_PLANE %ls COMBO_MID PLANE:RIGHT 40",name);CASE(0,L"DATUM_PLANE %ls COMBO_TOP PLANE:FRONT 20",name);CASE(0,L"SKETCH_CREATE %ls BOSS_SK PLANE:COMBO_TOP PLANE:RIGHT RECT 15 24 10 12",name);CASE(0,L"SOLID_EXTRUDE %ls BOSS BOSS_SK ADD BLIND 8 NEG",name);
 {int top=0,edge=0;ProFeature boss;ProSolidSurfaceVisit((ProSolid)model,ux_boss_top,NULL,&top);ux_check(top>0,"combo actual boss top face");CASE(0,L"HOLE %ls BOSS_HOLE SURFACE:%d SURFACE:%d 20 SURFACE:%d 30 4 THROUGH 0",name,top,f.x,f.y);if(!ux_named_feature(model,L"BOSS",&boss))ProFeatureGeomitemVisit(&boss,PRO_EDGE,ux_boss_edge,NULL,&edge);ux_check(edge>0,"combo actual boss vertical edge");CASE(0,L"EDGE_ROUND %ls BOSS_ROUND 1 EDGE:%d",name,edge);double before=volume(model);CASE(0,L"REUSE_MIRROR %ls MIRRORED_COMBO PLANE:COMBO_MID BOSS BOSS_HOLE BOSS_ROUND",name);ux_check(part_close(volume(model),96000+2*(before-96000)),"related boss hole and round mirrored together with actual volume");}
 NEW();HOLE(L"G1",10,10);HOLE(L"G2",30,20);ux_test_group_fail=1;CASE(1,L"REUSE_GROUP %ls REJECT_POSTCHECK G1 G2",name);ux_test_group_fail=0;CASE(0,L"REUSE_GROUP %ls GROUP_A G1 G2",name);expect_volume(model,96000-2*80*AS_PI);
 NEW();HOLE(L"F1",10,10);HOLE(L"MIDDLE",30,20);HOLE(L"F3",50,20);CASE(1,L"REUSE_GROUP %ls NONCONTIGUOUS F1 F3",name);CASE(0,L"REUSE_GROUP %ls COMPLETE_GROUP F1 MIDDLE F3",name);expect_volume(model,96000-3*80*AS_PI);
 NEW();
 {ProPath resource,source,directory,target;if(ProToolkitApplTextPathGet(resource)||ProDirectoryCurrentGet(directory)){failures++;goto done;}swprintf_s(source,PRO_PATH_SIZE,L"%ls\\udf\\bwtl_boundary.gph.1",resource);swprintf_s(target,PRO_PATH_SIZE,L"%ls\\SOURCE_TEMPLATE.gph",directory);if(!CopyFileW(source,target,TRUE)){trace("copy owned native UDF fixture",GetLastError());failures++;}}
 CASE(0,L"REUSE_UDF_SAVE %ls MY_TEMPLATE SOURCE_TEMPLATE",name);CASE(0,L"REUSE_UDF_INFO %ls MY_TEMPLATE",name);
 CASE(1,L"REUSE_UDF_SAVE %ls MY_TEMPLATE SOURCE_TEMPLATE",name);CASE(1,L"REUSE_UDF_INFO %ls NOT_INSTALLED",name);
 CASE(0,L"SKETCH_CREATE %ls UDF_FIRST PLANE:FRONT PLANE:RIGHT LINE 0 0 80 0",name);CASE(0,L"DATUM_PLANE %ls UDF_Z30 PLANE:FRONT 30",name);CASE(0,L"SKETCH_CREATE %ls UDF_SECOND PLANE:UDF_Z30 PLANE:RIGHT LINE 0 0 80 0",name);
 CASE(0,L"REUSE_UDF_PLACE %ls REUSED MY_TEMPLATE CURVE:UDF_FIRST CURVE:UDF_SECOND",name);expect_volume(model,96000);
 CASE(1,L"REUSE_UDF_PLACE %ls BAD_REFS MY_TEMPLATE CURVE:MISSING CURVE:UDF_SECOND",name);
 NEW();CASE(0,L"REUSE_PARAM %ls PART_CODE STRING BWTL_TEST",name);CASE(0,L"REUSE_PARAM %ls PITCH REAL 20",name);CASE(0,L"REUSE_PARAM %ls TIMES INTEGER 3",name);CASE(0,L"REUSE_PARAM %ls ENABLED BOOLEAN TRUE",name);CASE(0,L"REUSE_PARAM %ls GAUGE REAL 2 MM",name);CASE(0,L"REUSE_PARAM %ls LINK REAL 1",name);CASE(0,L"REUSE_RELATION %ls LINK PITCH*2+3",name);
 CASE(1,L"REUSE_PARAM %ls PITCH REAL 99",name);CASE(1,L"REUSE_RELATION %ls LINK PITCH+5",name);CASE(0,L"REUSE_PARAM %ls OTHER REAL 1",name);CASE(1,L"REUSE_RELATION %ls OTHER PITCH/0",name);CASE(1,L"REUSE_RELATION %ls OTHER DOES_NOT_EXIST*2",name);CASE(0,L"REUSE_RELATION %ls OTHER (PITCH+2)*3",name);
 CASE(0,L"REUSE_INFO %ls",name);
 {PartSnapshot *snapshot=calloc(1,sizeof(*snapshot));if(!snapshot){failures++;goto done;}ProSolidDimensionVisit((ProSolid)model,PRO_B_FALSE,part_dimension_visit,NULL,snapshot);int found=-1;for(int i=0;i<snapshot->dimensions;i++)if(part_close(snapshot->dimension[i].value,20)&&snapshot->dimension[i].type==PRODIMTYPE_LINEAR&&!snapshot->dimension[i].related)found=i;
  if(found<0)failures++;else {CASE(1,L"REUSE_FAMILY %ls BAD_FAMILY DIM:NOT_A_DIM 15 25",name);CASE(0,L"REUSE_FAMILY %ls UX_FAMILY DIM:%ls 15 25 30",name,snapshot->dimension[found].symbol);CASE(1,L"REUSE_FAMILY %ls UX_FAMILY DIM:%ls 15 25 30",name,snapshot->dimension[found].symbol);ux_family_disk_check(snapshot->dimension[found].symbol);}free(snapshot);}
done:trace("REUSE_CASES",cases);trace("REUSE_FAILURES",failures);trace("REUSE_DONE",1);ProEngineerEnd();return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
 uiCmdCmdId id;char marker[16];ProError e;if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
 HMODULE modules[2048];DWORD bytes;if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return PRO_TK_GENERAL_ERROR;
 for(int i=0;i<(int)(bytes/sizeof(HMODULE));i++){wchar_t path[MAX_PATH];GetModuleFileNameW(modules[i],path,MAX_PATH);CharLowerBuffW(path,(DWORD)wcslen(path));if(wcsstr(path,L"creo_safe_resident"))return PRO_TK_BAD_CONTEXT;}trace("legacy_bridge_loaded",0);
 if((e=ProCmdActionAdd("BWTLReuseProbe",ux_test_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;return ProMacroLoad(L"~ Command `BWTLReuseProbe`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
