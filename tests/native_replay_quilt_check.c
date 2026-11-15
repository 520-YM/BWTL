#define BWTL_REUSE_QUILT_EDITS
#include "native_replay_quilt_edits.c"
#undef BWTL_REUSE_QUILT_EDITS

static ProError quilt_profile_fixture(ProMdl *model,double width,double height,FILE *log)
{
    ProMdlName name;
    const char *stage="prepare contour comparison fixture";
    ProError error=model_create_empty(1,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    wchar_t command[512];
    swprintf_s(command,_countof(command),L"CREO_OP SKETCH_CREATE %ls PATCH_SK PLANE:FRONT PLANE:RIGHT RECT 0 0 60 40",name);
    if(!error)error=expansion_build(*model,command,log);
    swprintf_s(command,_countof(command),L"CREO_OP SURFACE_FILL %ls PATCH PATCH_SK",name);
    if(!error)error=expansion_build(*model,command,log);
    swprintf_s(command,_countof(command),L"CREO_OP DATUM_PLANE %ls DETAIL_PLANE PLANE:FRONT 10",name);
    if(!error)error=expansion_build(*model,command,log);
    swprintf_s(command,_countof(command),L"CREO_OP SKETCH_CREATE %ls DETAIL_SK PLANE:DETAIL_PLANE PLANE:RIGHT RECT 0 0 %.8g %.8g",name,width,height);
    if(!error)error=expansion_build(*model,command,log);
    swprintf_s(command,_countof(command),L"CREO_OP SURFACE_FILL %ls DETAIL DETAIL_SK",name);
    if(!error)error=expansion_build(*model,command,log);
    return error;
}

static ProError quilt_local_difference(FILE *log)
{
    ProMdl original=NULL,changed=NULL;
    ProError error=quilt_profile_fixture(&original,10,15,log);
    if(!error)error=quilt_profile_fixture(&changed,15,10,log);
    ImitationShapeMetrics a,b;
    if(!error)error=imitation_shape_read(original,&a);
    if(!error)error=imitation_shape_read(changed,&b);
    if(!error&&(a.quilts!=2||b.quilts!=2||!imitation_metric_same(a.quilt_area,b.quilt_area)||
       !ad_near(a.bounds[0],b.bounds[0],0.001)||!ad_near(a.bounds[1],b.bounds[1],0.001)))error=PRO_TK_INCOMPLETE;
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText out={report,_countof(report),0};
    if(!error)error=imitation_shape_verify_impl(original,changed,&a,0,1,&out);
    const int detected=error==PRO_TK_INCOMPLETE&&wcsstr(report,L"EDGE:")&&wcsstr(report,L"轮廓或连接差异");
    fwprintf(log,L"SAME AREA AND ENVELOPE, MOVED LOCAL BOUNDARY=%d\n%ls\n",detected,report);
    return detected?PRO_TK_NO_ERROR:PRO_TK_INCOMPLETE;
}

static int quilt_check_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-quilt-check.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    setvbuf(log,NULL,_IONBF,0);
    model_background_mode=1;
    ProError joined=quilt_trial(3,log);
    ProError local=quilt_local_difference(log);
    fwprintf(log,L"JOINED=%d LOCAL=%d PASS=%d\n",joined,local,!joined&&!local);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80])
{
    uiCmdCmdId id;
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    if(!error)error=ProCmdActionAdd("BWTLReplayQuiltCheck",quilt_check_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayQuiltCheck\x0060;");
}
