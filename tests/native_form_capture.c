#define BWTL_REUSE_MACHINING_FIXTURE
#include "native_machining.c"
#include <ProUICmd.h>
#include <ProMenu.h>
#include <ProNotify.h>
#include <ProSelbuffer.h>
static ProMdl capture_model;
static ProError capture_post(ProFeature *feature){
    ProFeattype type;ProElement tree=NULL;
    if(feature->owner!=capture_model||ProFeatureTypeGet(feature,&type)||type!=PRO_FEAT_SMT_SKETCH_FORM)return 0;
    if(!ProFeatureElemtreeExtract(feature,NULL,PRO_FEAT_EXTRACT_NO_OPTS,&tree)){ProPath path=L"native-ui-form.xml";ProElemtreeWrite(tree,PRO_ELEMTREE_XML,path);ProFeatureElemtreeFree(feature,tree);}
    trace("UI form material volume",(int)volume(capture_model));return 0;
}
static ProError capture_face(ProSurface surface,ProError status,ProAppData data){
    ProSmtSurfType type;int *id=data;if(!status&&!ProSmtSurfaceTypeGet((ProPart)capture_model,surface,&type)&&type==PRO_SMT_SURF_FACE)ProSurfaceIdGet(surface,id);return 0;
}
static int capture_open(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProFeature sketch;ProSelection selection;ProName name=L"FORM_CIRCLE";
    if(!ProModelitemByNameInit(capture_model,PRO_FEATURE,name,(ProModelitem *)&sketch)&&!ProSelectionAlloc(NULL,(ProModelitem *)&sketch,&selection)){
        ProSelbufferClear();ProSelbufferSelectionAdd(selection);ProSelectionFree(&selection);
    }
    return ProMacroLoad(L"~ Command `ProCmdSmtSketchForm`;");
}
static int capture_prepare(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdlName name;const char *stage="capture fixture";wchar_t cmd[1024];int face=0;model_background_mode=0;
    ProError e=model_create_empty(2,name,&stage);if(!e)e=ProMdlnameInit(name,PRO_MDL_PART,&capture_model);if(e){trace(stage,e);return 0;}
    swprintf_s(cmd,1024,L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);if(run(capture_model,cmd,0))return 0;
    swprintf_s(cmd,1024,L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name);if(run(capture_model,cmd,0))return 0;
    ProSolidSurfaceVisit((ProSolid)capture_model,capture_face,NULL,&face);
    swprintf_s(cmd,1024,L"CREO_OP SMT_SKETCH %ls FORM_CIRCLE SURFACE:%d PLANE:RIGHT CIRCLE 40 -30 8",name,face);if(run(capture_model,cmd,0))return 0;
    ProMdlnameRename(capture_model,L"BWTL_FORM_CAPTURE");ProMdlDisplay(capture_model);
    ProNotificationSet(PRO_FEATURE_CREATE_POST,(ProFunction)capture_post);
    return ProMacroLoad(L"~ Command `BWTLFormOpen`;");
}
__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    uiCmdCmdId id;char marker[16];ProError e;
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",marker,16)||strcmp(marker,"yes"))return PRO_TK_BAD_CONTEXT;
    if((e=ProCmdActionAdd("BWTLFormOpen",capture_open,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;
    if((e=ProCmdActionAdd("BWTLFormPrepare",capture_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id)))return e;
    return ProMacroLoad(L"~ Command `BWTLFormPrepare`;");
}
__declspec(dllexport) void user_terminate(void){foundation_reset();}
