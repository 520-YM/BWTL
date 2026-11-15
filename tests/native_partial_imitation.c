#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProMdl partial_source;
static ProWVerstamp partial_before;
static ImitationOutcome partial_outcome;
static ProPath partial_delivered;
static FILE *partial_log;

static int partial_imitation_verify(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProWVerstamp after=NULL;
    int same=partial_source&&partial_before&&!ProMdlVerstampGet(partial_source,&after)&&ProVerstampEqual(partial_before,after);
    if(partial_before)ProVerstampFree(&partial_before);
    if(after)ProVerstampFree(&after);
    ProMdl target=NULL;
    ReplayOrder order={0};
    ProError current=ProMdlCurrentGet(&target);
    ProMdlName name=L"";
    if(!current)ProMdlMdlnameGet(target,name);
    fwprintf(partial_log,L"ACTIVE=%ls CREO=%d\n",name,current);
    if(partial_outcome.partial_displayed&&!current&&target!=partial_source){
        order.read_candidate=1;
        ProError read=ProSolidFeatVisit((ProSolid)target,replay_read_order,NULL,&order);
        fwprintf(partial_log,L"READ_ORDER=%d COUNT=%d\n",read,order.count);
        if(read)order.count=0;
        replay_pattern_source_close(&order.patterns);
        imitation_write_progress(partial_log,target);
    }
    int kept=partial_outcome.partial_displayed&&target&&target!=partial_source&&order.count==2&&
        order.step[0].type==PRO_FEAT_DATUM_SURF&&order.step[1].type==PRO_FEAT_CUT_SMT&&
        !partial_delivered[0]&&!part_health(target);
    fwprintf(partial_log,L"ORIGINAL_UNCHANGED=%d PREFIX=%d STEPS=%d PASS=%d\n",same,kept,order.count,
        same&&kept&&partial_outcome.replay.step==3);
    fclose(partial_log);
    ProEngineerEnd();
    return 0;
}

static int partial_imitation_test(uiCmdCmdId command,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"partial-imitation.log",L"wt, ccs=UTF-8");
    if(!log){ProEngineerEnd();return 0;}
    ProPath path,delivered=L"";
    ProMdl source=NULL;
    ProWVerstamp before=NULL;
    ProError error=GetEnvironmentVariableW(L"BWTL_EXECUTION_FIXTURE",path,_countof(path))?
        ProMdlFiletypeLoad(path,PRO_MDLFILE_PART,PRO_B_FALSE,&source):PRO_TK_E_NOT_FOUND;
    if(!error)error=ProMdlVerstampGet(source,&before);
    wchar_t report[PART_TEXT_MAX]=L"";
    PartText receipt={report,_countof(report),0};
    ImitationOutcome outcome={0};
    if(!error)error=imitation_run_mode_impl(source,delivered,1,0,&outcome,&receipt);
    fwprintf(log,L"REPLAY=%d COMPLETED=%d FAILED_STEP=%d PARTIAL=%d\n%ls\n",
        error,imitation_last_completed,outcome.replay.step,outcome.partial_displayed,report);
    fflush(log);
    partial_source=source;
    partial_before=before;
    partial_outcome=outcome;
    wcscpy_s(partial_delivered,_countof(partial_delivered),delivered);
    partial_log=log;
    if(ProMacroLoad(L"~ Command `BWTLPartialImitationVerify`;"))return partial_imitation_verify(0,NULL,NULL);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLPartialImitationTest",partial_imitation_test,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLPartialImitationVerify",partial_imitation_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLPartialImitationTest`;");
}