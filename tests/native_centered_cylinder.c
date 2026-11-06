/* Test the actual menu template and native chat dispatch without an AI engine. */
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl original_model,first_cylinder;
static ProWVerstamp original_stamp;
static int probe_queued,probe_phase;
static ULONGLONG probe_started;

static void probe_log(const char *label,int passed){
    FILE *f=fopen("centered-cylinder-result.log","a");
    if(f){fprintf(f,"%s: %s\n",label,passed?"PASS":"FAIL");fclose(f);}
}

static int probe_end(int passed){
    probe_log("CENTERED_CYLINDER_NATIVE",passed);
    if(original_stamp)ProVerstampFree(&original_stamp);
    KillTimer(native_backend_window,5001);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static void CALLBACK probe_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!probe_queued)probe_queued=ProMacroLoad(L"~ Command `BWTLCenteredProbeTick`;")==PRO_TK_NO_ERROR;
}

static int probe_verify(ProMdl model,double diameter,double height){
    ProModelitem sketch,extrusion;
    ProMassProperty mass;Pro3dPnt outline[2];int *parents=NULL,count=0,i,linked=0;
    if(ProModelitemByNameInit(model,PRO_FEATURE,L"CYL_PROFILE",&sketch)||
       ProModelitemByNameInit(model,PRO_FEATURE,L"CYL_ADD",&extrusion)||
       ProFeatureParentsGet((ProFeature *)&extrusion,&parents,&count))return 0;
    for(i=0;i<count;i++)if(parents[i]==sketch.id)linked=1;
    if(parents)ProArrayFree((ProArray *)&parents);
    return linked&&!part_health(model)&&!ProSolidOutlineGet((ProSolid)model,outline)&&
        part_close(outline[0][2],0)&&part_close(outline[1][2],height)&&
        !ProSolidMassPropertyWithDensityGet((ProSolid)model,NULL,PRO_MP_DENS_USE_ALWAYS,1,&mass)&&
        part_close(mass.volume,3.141592653589793*diameter*diameter*height/4);
}

static int probe_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl active;ProWVerstamp stamp=NULL;
    int same_stamp;
    probe_queued=0;
    if(GetTickCount64()-probe_started>60000||ai_process)return probe_end(0);
    if(smart_active)return 0;
    if(ProMdlCurrentGet(&active))return probe_end(0);
    if(probe_phase==0){
        const NativeFeature *entry=NULL;size_t i;wchar_t input[128];
        if(active!=original_model||open_window(0,NULL,NULL))return probe_end(0);
        for(i=0;i<_countof(native_features);i++)if(!wcscmp(native_features[i].label,L"原点居中圆柱"))entry=&native_features[i];
        if(!entry||entry->mode!=FEATURE_SMART||!wcsstr(entry->draft,L"新建原点居中圆柱 "))return probe_end(0);
        probe_log("menu offers the validated native command",1);
        native_mode=1;native_prompt_set(L"新建原点居中圆柱 150 150");run_local(NULL,NULL,NULL);
        if(smart_active||ai_process||ProMdlCurrentGet(&active)||active!=original_model)return probe_end(0);
        probe_log("analyze-only blocks the direct operation",1);
        native_mode=0;native_prompt_set(L"新建原点居中圆柱 0 150");run_local(NULL,NULL,NULL);
        if(smart_active||ai_process||ProMdlCurrentGet(&active)||active!=original_model)return probe_end(0);
        probe_log("invalid diameter leaves active part unchanged",1);
        if(ProMdlVerstampGet(original_model,&original_stamp))return probe_end(0);
        swprintf_s(input,_countof(input),L"%.*ls150 150",(int)(wcschr(entry->draft,L' ')-entry->draft+1),entry->draft);
        probe_phase=1;native_prompt_set(input);run_local(NULL,NULL,NULL);
        return 0;
    }
    probe_log("new cylinder is the active model",active!=original_model);
    if(active==original_model||ProMdlVerstampGet(original_model,&stamp))return probe_end(0);
    same_stamp=ProVerstampEqual(original_stamp,stamp);ProVerstampFree(&stamp);
    probe_log("original stamp preserved after activating new part",same_stamp);
    if(!same_stamp)return probe_end(0);
    if(probe_phase==1){
        int geometry_ok=probe_verify(active,150,150),reply_ok=wcsstr(history,L"已创建原点居中圆柱")!=NULL;
        probe_log("first geometry check",geometry_ok);probe_log("one native reply received",reply_ok);
        if(!geometry_ok||!reply_ok)return probe_end(0);
        probe_log("150 mm sample matches +Z and exact volume with editable sketch",1);
        first_cylinder=active;
        probe_phase=2;native_prompt_set(L"新建原点居中圆柱 42.5 17.25");run_local(NULL,NULL,NULL);
        return 0;
    }
    if(active==first_cylinder||!probe_verify(active,42.5,17.25))return probe_end(0);
    probe_log("second request respects different decimal dimensions",1);
    probe_log("original part stamp unchanged",1);
    return probe_end(1);
}

static int probe_begin(uiCmdCmdId command,uiCmdValue *value,void *data){
    const char *stage="fixture";
    ProMdlName name;
    ModelPlan prior={1,25,25,12,0,1};
    if(model_create(&prior,name,&stage)||ProMdlnameInit(name,PRO_MDL_PART,&original_model))return probe_end(0);
    probe_started=GetTickCount64();
    return SetTimer(native_backend_window,5001,1000,probe_timer)?0:probe_end(0);
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error;uiCmdCmdId command;char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,16)||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    error=ProCmdActionAdd("BWTLCenteredProbeBegin",probe_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    error=ProCmdActionAdd("BWTLCenteredProbeTick",probe_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(error)return error;
    return ProMacroLoad(L"~ Command `BWTLCenteredProbeBegin`;");
}
