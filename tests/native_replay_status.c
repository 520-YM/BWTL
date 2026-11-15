#define BWTL_BATCH_REPORTING_HELPER
#include "native_imitation_batch_reporting.c"
#undef BWTL_BATCH_REPORTING_HELPER

static ProError status_toggle(ProFeature *feature, int suppress)
{
    int *ids=NULL;
    ProFeatureDeleteOptions *del=NULL;
    ProFeatureResumeOptions *resume=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*ids),1,(ProArray *)&ids);
    if(!error)error=ProArrayAlloc(1,sizeof(*del),1,(ProArray *)&del);
    if(!error)error=ProArrayAlloc(1,sizeof(*resume),1,(ProArray *)&resume);
    if(!error){
        ids[0]=feature->id;
        del[0]=PRO_FEAT_DELETE_NO_OPTS;
        resume[0]=PRO_FEAT_RESUME_NO_OPTS;
        error=suppress?ProFeatureWithoptionsSuppress((ProSolid)feature->owner,ids,del,
            PRO_REGEN_NO_RESOLVE_MODE|PRO_REGEN_UNDO_IF_FAIL):
            ProFeatureWithoptionsResume((ProSolid)feature->owner,ids,resume,PRO_REGEN_NO_RESOLVE_MODE);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(del)ProArrayFree((ProArray *)&del);
    if(resume)ProArrayFree((ProArray *)&resume);
    return error;
}

static ProError status_case(FILE *log)
{
    ModelPlan plan={0,100,80,20,0,1};
    ProMdlName name;
    ProMdl source=NULL;
    const char *stage=NULL;
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    wchar_t protocol[512],report[16000]=L"";
    PartText out={report,_countof(report),0};
    ProPath delivered=L"";
    if(!error){
        PartCommand command={0};
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DATUM_PLANE %ls STATUS_PLANE PLANE:FRONT 18",name);
        error=part_command_parse(protocol,&command)==1?
            foundation_execute(&command,source,report,_countof(report)):PRO_TK_BAD_INPUTS;
    }
    ProFeature feature;
    if(!error)error=ux_named_feature(source,L"STATUS_PLANE",&feature);
    if(!error)error=status_toggle(&feature,1);
    report[0]=0;
    if(!error)replay_preview(source,&out);
    if(!error&&(!wcsstr(report,L"拉伸加料")||!wcsstr(report,L"STATUS_PLANE")||!wcsstr(report,L"手动抑制")||
                !wcsstr(report,L"当前不可重建")||!wcsstr(report,L"抑制 1")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SUPPRESSED PREVIEW=%d\n%ls\n",error,report);
    out=(PartText){report,_countof(report),0};
    report[0]=0;
    if(!error&&(imitation_run_mode(source,delivered,0,1,&out)!=PRO_TK_BAD_CONTEXT||
                delivered[0]||!wcsstr(report,L"手动抑制")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"SUPPRESSED REPLAY=%d\n%ls\n",error,report);
    if(!error)error=status_toggle(&feature,0);
    out=(PartText){report,_countof(report),0};
    report[0]=0;
    if(!error)replay_preview(source,&out);
    if(!error&&(!wcsstr(report,L"STATUS_PLANE")||wcsstr(report,L"当前不可重建")||
                !wcsstr(report,L"可尝试重建")||!wcsstr(report,L"抑制 0")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"RESUMED PREVIEW=%d\n%ls\n",error,report);
    out=(PartText){report,_countof(report),0};
    report[0]=0;
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    if(!error&&(!delivered[0]||!wcsstr(report,L"新特征 STATUS_PLANE")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"RESUMED REPLAY=%d\n%ls\n",error,report);
    if(!error&&(!replay_feature_issue(PRO_FEAT_ACTIVE,PRO_FEAT_STAT_FAILED)||
                !replay_feature_issue(PRO_FEAT_ACTIVE,PRO_FEAT_STAT_CHILD_OF_FAILED)||
                !replay_feature_issue(PRO_FEAT_ACTIVE,PRO_FEAT_STAT_CHILD_OF_EXT_FAILED)||
                !replay_feature_issue(PRO_FEAT_ACTIVE,PRO_FEAT_STAT_UNREGENERATED)||
                replay_feature_issue(PRO_FEAT_ACTIVE,0)))error=PRO_TK_INCOMPLETE;
    return error;
}

static ProError status_frozen(FILE *log)
{
    ProPath path;
    ProMdl source=NULL;
    ProError error=GetEnvironmentVariableW(L"BWTL_PREVIEW_FROZEN",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_BAD_INPUTS;
    wchar_t report[20000]=L"";
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&(!wcsstr(report,L"平面首壁")||!wcsstr(report,L"钣金拉伸切口")||
                !wcsstr(report,L"外部参照可用")||!wcsstr(report,L"需核对外部参照")||
                !wcsstr(report,L"外部参照阻断")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"FROZEN PREVIEW=%d\n%ls\n",error,report);
    return error;
}

static ProError status_delete_test_parent(ProMdl model,ProFeature *feature)
{
    int *ids=NULL;
    ProFeatureDeleteOptions *options=NULL;
    ProError error=ProArrayAlloc(1,sizeof(*ids),1,(ProArray *)&ids);
    if(!error)error=ProArrayAlloc(1,sizeof(*options),1,(ProArray *)&options);
    if(!error){
        ids[0]=feature->id;
        options[0]=PRO_FEAT_DELETE_NO_OPTS;
        error=ProFeatureWithoptionsDelete((ProSolid)model,ids,options,PRO_REGEN_NO_RESOLVE_MODE);
    }
    if(ids)ProArrayFree((ProArray *)&ids);
    if(options)ProArrayFree((ProArray *)&options);
    return error;
}

static ProError status_failed_datum(FILE *log)
{
    ModelPlan plan={0,100,80,20,0,1};
    ProMdlName name;
    ProMdl source=NULL;
    const char *stage=NULL;
    ProError error=model_create(&plan,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&source);
    wchar_t protocol[512],report[20000]=L"";
    const wchar_t *steps[]={
        L"CREO_OP DATUM_PLANE %ls KEEP_PARENT PLANE:RIGHT 9",
        L"CREO_OP DATUM_PLANE %ls LOSE_PARENT PLANE:RIGHT 5",
        L"CREO_OP DATUM_PLANE %ls BROKEN_PLANE PLANE:LOSE_PARENT 4",
        L"CREO_OP DATUM_PLANE %ls CHILD_PLANE PLANE:BROKEN_PLANE 6"
    };
    for(int i=0;!error&&i<_countof(steps);i++){
        PartCommand command={0};
        swprintf_s(protocol,_countof(protocol),steps[i],name);
        error=part_command_parse(protocol,&command)==1?
            foundation_execute(&command,source,report,_countof(report)):PRO_TK_BAD_INPUTS;
    }
    ProFeature lost,broken,child;
    if(!error)error=ux_named_feature(source,L"LOSE_PARENT",&lost);
    if(!error)error=ux_named_feature(source,L"BROKEN_PLANE",&broken);
    if(!error)error=ux_named_feature(source,L"CHILD_PLANE",&child);
    if(!error)error=status_delete_test_parent(source,&lost);
    unsigned int broken_flags=0,child_flags=0;
    if(!error)error=ProFeatureStatusflagsGet(&broken,&broken_flags);
    if(!error)error=ProFeatureStatusflagsGet(&child,&child_flags);
    if(!error&&(!(broken_flags&PRO_FEAT_STAT_FAILED)||
       !(child_flags&PRO_FEAT_STAT_CHILD_OF_FAILED)))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"NATIVE FAILURE=%d BROKEN=%u CHILD=%u\n",error,broken_flags,child_flags);
    ProWVerstamp before=NULL,after=NULL;
    if(!error)error=ProMdlVerstampGet(source,&before);
    report[0]=0;
    PartText out={report,_countof(report),0};
    if(!error)replay_preview(source,&out);
    if(!error&&(!wcsstr(report,L"BROKEN_PLANE")||!wcsstr(report,L"设计状态 再生失败")||
                !wcsstr(report,L"设计状态 父特征再生失败")))error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"NATIVE FAILURE PREVIEW=%d\n%ls\n",error,report);
    ProPath delivered=L"";
    report[0]=0;
    out.truncated=0;
    ImitationOutcome outcome={0};
    if(!error&&(imitation_run_mode_impl(source,delivered,0,1,&outcome,&out)!=PRO_TK_BAD_CONTEXT||
                delivered[0]||outcome.replay.feature!=broken.id||!wcsstr(report,L"BROKEN_PLANE")))
        error=PRO_TK_INCOMPLETE;
    if(!error)error=ProMdlVerstampGet(source,&after);
    if(!error&&!ProVerstampEqual(before,after))error=PRO_TK_INCOMPLETE;
    if(before)ProVerstampFree(&before);
    if(after)ProVerstampFree(&after);
    fwprintf(log,L"NATIVE FAILURE BLOCK=%d\n%ls\n",error,report);
    report[0]=0;
    out.truncated=0;
    if(!error){
        PartCommand command={0};
        swprintf_s(protocol,_countof(protocol),L"CREO_OP DV_REF %ls BROKEN_PLANE MISSING PLANE:KEEP_PARENT",name);
        error=part_command_parse(protocol,&command)==1?dv_redefine(source,&command,&out):PRO_TK_BAD_INPUTS;
    }
    fwprintf(log,L"NATIVE FAILURE REPAIR=%d\n%ls\n",error,report);
    report[0]=0;
    out.truncated=0;
    if(!error)error=imitation_run_mode(source,delivered,0,1,&out);
    if(!error&&!delivered[0])error=PRO_TK_INCOMPLETE;
    fwprintf(log,L"NATIVE FAILURE RECOVERED=%d\n%ls\n",error,report);
    return error;
}

static int status_test(uiCmdCmdId id,uiCmdValue *value,void *data)
{
    FILE *log=_wfopen(L"replay-status.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    model_background_mode=1;
    const ProError state=status_case(log);
    const ProError unsupported=batch_feature_failure(log);
    const ProError frozen=status_frozen(log);
    const ProError failed=status_failed_datum(log);
    fwprintf(log,L"STATUS=%d UNSUPPORTED=%d FROZEN=%d FAILED=%d PASS=%d\n",
        state,unsupported,frozen,failed,!state&&!unsupported&&!frozen&&!failed);
    fclose(log);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t message[80])
{
    ProError error=imitation_batch_reporting_initialize(argc,argv,version,build,message);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLReplayStatus",status_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLReplayStatus\x0060;");
}
