#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
static ProMdl seed_model;
static ProError seed_feature(ProFeature *f,ProError status,ProAppData data){
    ProFeattype type;
    int subtype=0;
    ProElement tree=NULL;
    ProPath file;
    ProError e;
    if(status)return status;
    if((e=ProFeatureTypeGet(f,&type)))return e;
    ProFeatureSubtypeGet(f,&subtype);
    trace("seed feature id",f->id);trace("seed feature type",type);trace("seed feature subtype",subtype);
    swprintf_s(file,PRO_PATH_SIZE,L"seed_%d.xml",f->id);
    e=ProFeatureElemtreeExtract(f,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree);
    trace("seed feature extraction",e);
    if(!e){ProElemtreeWrite(tree,PRO_ELEMTREE_XML,file);ProFeatureElemtreeFree(f,tree);}
    return PRO_TK_NO_ERROR;
}
static int seed_done(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProSolidFeatVisit((ProSolid)seed_model,seed_feature,NULL,NULL);
    ProMdlSave(seed_model);
    trace("SEED_DONE",1);return 0;
}
static int seed_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdlName name;
    const char *stage="seed empty";
    ProError e;
    wchar_t request[1024];
    model_background_mode=0;
    if((e=model_create_empty(1,name,&stage))||(e=ProMdlnameInit(name,PRO_MDL_PART,&seed_model)))return e;
    trace("seed inspection mapkey",ProConfigoptSet(L"mapkey",L"zq @MAPKEY_LABELinspect surface seed;~ Command `BWTLSurfaceSeedDone`;"));
    swprintf_s(request,_countof(request),L"CREO_OP SKETCH_CREATE %ls BOUND_A PLANE:FRONT PLANE:RIGHT LINE 0 0 20 0",name);run(seed_model,request,0);
    swprintf_s(request,_countof(request),L"CREO_OP SKETCH_CREATE %ls BOUND_B PLANE:FRONT PLANE:RIGHT ARC 10 10 10 0 180",name);run(seed_model,request,0);
    e=ProMacroLoad(
        L"~ Select `main_dlg_cur` `PHTLeft.AssyTree` 2 `T3 5` `T3 7`;"
        L"~ Command `ProCmdFtBndBlend`; ~ Activate `main_dlg_cur` `dashInst0.Done`;"
        L"~ Command `ProCmdMmUDFLib`; #CREATE; bwtl_boundary; #STAND ALONE; #DONE; no;"
        L"~ Select `main_dlg_cur` `PHTLeft.AssyTree` 1 `T3 8`; ~ Activate `GET SELECT` `done_sel`;"
        L"#DONE; #DONE/RETURN; FIRST_CURVE; SECOND_CURVE; #DONE/RETURN; ~ Activate `Odui_Dlg_01` `okay`;"
        L"~ Command `BWTLSurfaceSeedDone`;");
    trace("seed macro requested",e);return 0;
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId command;
    char marker[16];
    ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,sizeof(marker))||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLSurfaceSeedDone",seed_done,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command))||
       (e=ProCmdActionAdd("BWTLSurfaceSeedBegin",seed_begin,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command)))return e;
    return ProMacroLoad(L"~ Command `BWTLSurfaceSeedBegin`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
