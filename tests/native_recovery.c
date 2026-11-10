#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int checks,failures;
static void recovery_check(int pass,const char *label){
    checks++;if(!pass)failures++;
    FILE *f=fopen("recovery-native.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int recovery_action(ProMdl model,const wchar_t *protocol){
    if(part_command_parse(protocol,&pending_part)!=1)return 0;
    if(part_snapshot_take_mode(model,&request_snapshot,0))return 0;
    execute_part_command();
    return 1;
}

static ProError recovery_solid(ProMdl *model,ProMdlName name){
    const ModelPlan stock={0,100,70,2,0,1};
    const char *stage;
    ProError error=model_create(&stock,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,model);
    int window;
    if(!error)error=ProMdlWindowGet(*model,&window);
    if(!error)error=ProWindowCurrentSet(window);
    return error;
}

static ProMdl live_model;
static int live_queued;
static ULONGLONG live_started;

static void CALLBACK live_timer(HWND window,UINT message,UINT_PTR id,DWORD now){
    if(!live_queued)live_queued=ProMacroLoad(L"~ Command `BWTLRecoveryLiveTick`;")==PRO_TK_NO_ERROR;
}
static int live_tick(uiCmdCmdId id,uiCmdValue *value,void *data){
    live_queued=0;
    if((smart_active||ai_process)&&GetTickCount64()-live_started<240000)return 0;
    KillTimer(native_backend_window,5401);
    ProMassProperty mass;
    int valid=!qa_geometry_mass(live_model,&mass)&&fabs(mass.volume-(14000-36*AS_PI))<0.02;
    recovery_check(valid&&smart_steps>=3&&smart_progress==2&&smart_total_failures==1,
        "real Codex reads failed receipt and generates the remaining native hole");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    recovery_check(answer&&!wcsstr(answer+4,L"【AI】"),"real Codex recovery produces one final reply");
    FILE *f=fopen("recovery-live.log","a");
    if(f){fprintf(f,"LIVE_FAILURES: %d\nLIVE_STEPS: %d\nLIVE_DONE: 1\n",failures,smart_steps);fclose(f);}
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int live_start(ProMdl model,const wchar_t *name,int face){
    wchar_t codex[MAX_PATH],goal[820],protocol[620];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
       L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)){
        recovery_check(0,"launch private AI test environment");
        return live_tick(0,NULL,NULL);
    }
    history[0]=0;
    live_model=model;live_started=GetTickCount64();
    swprintf_s(goal,_countof(goal),L"在当前钣金零件 %ls 上先开第一个按钮孔，然后再开第二个按钮孔。"
        L"板厚2、两孔直径均为6，无缺口。第一个特征名 HOLE_FIRST，中心(25,35)，第二个特征名 HOLE_SECOND，中心(55,35)。"
        L"入口实际实体面 SURFACE:%d，参考 CSYS:WORK_AT 和 PLANE:RIGHT。请按已有原生回执继续剩余步骤，不保存。",name,face);
    smart_begin(goal);
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls HOLE_FIRST SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 25 35 2 6 0 0",name,face);
    if(!recovery_action(model,protocol)||smart_steps!=1||smart_progress!=1){
        recovery_check(0,"seed first real native feature");return live_tick(0,NULL,NULL);
    }
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls HOLE_SECOND SURFACE:999999 CSYS:WORK_AT PLANE:RIGHT 55 35 2 6 0 0",name);
    if(!recovery_action(model,protocol)||!smart_active||smart_total_failures!=1){
        recovery_check(0,"seed a failed operation with unchanged native model");return live_tick(0,NULL,NULL);
    }
    if(!SetTimer(native_backend_window,5401,1000,live_timer)){
        recovery_check(0,"arm live test timeout");smart_cancel();return live_tick(0,NULL,NULL);
    }
    return 0;
}

static int recovery_fixture(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlName name;
    ProPath work,alternate;
    FdTxn tx={0};
    wchar_t protocol[620],first[620],receipt[PART_TEXT_MAX];
    int face=0;
    ProError error=recovery_solid(&model,name);
    if(!error)error=recovery_solid(&model,name);
    if(!error)error=ProDirectoryCurrentGet(work);
    if(error){recovery_check(0,"warm up private Creo and read workspace");goto done;}
    recovery_check(1,"warm up private Creo and read workspace");
    tx.model=model;tx.scale=1;
    error=quick_top(&tx,2,&face);
    if(!error)error=am_sub(model,L"CREO_OP DATUM_CSYS %ls WORK_AT CSYS:PRT_CSYS_DEF 0 0 2 0 0 0",name);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_CONVERT %ls TO_SHEET SURFACE:%d 2",name,face);
    if(!error)error=quick_top(&tx,2,&face);
    recovery_check(!error&&face,"create and bind native sheet fixture");
    if(error||!face)goto done;
    if(GetEnvironmentVariableW(L"BWTL_RECOVERY_LIVE",NULL,0))return live_start(model,name,face);

    smart_begin(L"在当前钣金零件上开第一个按钮孔，然后再开两个按钮孔。");
    recovery_check(smart_chain&&!smart_readonly,"recognize an explicit multi-step native modeling goal");
    swprintf_s(first,_countof(first),L"CREO_OP PN_BUTTON %ls HOLE_FIRST SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 25 35 2 6 0 0",name,face);
    recovery_check(recovery_action(model,first)&&smart_active&&smart_steps==1&&smart_progress==1&&smart_binding_valid(),
        "a native panel feature succeeds and keeps the chain alive");

    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls HOLE_SECOND SURFACE:999999 CSYS:WORK_AT PLANE:RIGHT 55 35 2 6 0 0",name);
    recovery_check(recovery_action(model,protocol)&&smart_active&&smart_steps==2&&smart_total_failures==1&&smart_binding_valid(),
        "one failed panel operation remains resumable with unchanged version");
    recovery_check(wcsstr(smart_details,L"第 2 步｜失败｜PN_BUTTON")&&wcsstr(smart_details,L"SURFACE:999999")&&wcsstr(smart_details,L"Creo -"),
        "failed native receipt records step, operation, reference and code");
    PartCommand repeated={0};
    recovery_check(part_command_parse(protocol,&repeated)==1&&smart_protocol_seen(&repeated),
        "the exact failed native operation is detected by the ledger");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls RENAMED SURFACE:999999 CSYS:WORK_AT PLANE:RIGHT 55 35 2 6 0 0",name);
    recovery_check(recovery_action(model,protocol)&&smart_active&&smart_steps==2&&wcsstr(smart_feedback,L"拦截重复"),
        "the same failed opening with only a new feature name is blocked");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls HOLE_SECOND SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 55 35 2 6 0 0",name,face);
    recovery_check(recovery_action(model,protocol)&&smart_active&&smart_steps==3&&smart_progress==2&&!smart_failures_without_progress,
        "alternative native reference succeeds without restarting");

    smart_stop();
    recovery_check(smart_paused&&!smart_active&&smart_steps==3,"stop retains the verified checkpoint");
    swprintf_s(alternate,_countof(alternate),L"%ls\\recovery-other",work);
    int switched=CreateDirectoryW(alternate,NULL)&&!ProDirectoryChange(alternate);
    int rejected=switched&&!smart_resume()&&smart_paused;
    error=ProDirectoryChange(work);
    recovery_check(rejected,"another working directory cannot resume the task");
    recovery_check(!error&&smart_resume()&&smart_active&&smart_steps==3&&smart_total_failures==1,
        "explicit resume retains steps, completed operations and total failures");

    recovery_check(recovery_action(model,first)&&smart_steps==3&&smart_active,
        "successful prior operation cannot be replayed after resume");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls HOLE_THIRD SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 80 35 2 6 0 0",name,face);
    recovery_check(recovery_action(model,protocol)&&smart_steps==4&&smart_progress==3&&smart_binding_valid(),
        "resume completes only the remaining native feature");
    ProMassProperty mass;
    recovery_check(!qa_geometry_mass(model,&mass)&&fabs(mass.volume-(14000-54*AS_PI))<0.02,
        "native volume has precisely three holes");
    recovery_check(store_read(L"last-native-details.txt",receipt,_countof(receipt))&&wcsstr(receipt,L"第 2 步｜失败")&&
        wcsstr(receipt,L"第 4 步｜成功"),"complete result ledger is stored in the private profile");
    smart_stop();
    PartCommand external={0};
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls EXTERNAL_EDIT SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 40 54 2 6 0 0",name,face);
    error=part_command_parse(protocol,&external)==1?foundation_execute(&external,model,receipt,_countof(receipt)):PRO_TK_BAD_INPUTS;
    recovery_check(!error&&!smart_resume(),"manual model version change rejects resume");
    smart_cancel();

    smart_begin(L"在当前钣金零件上开一个按钮孔");
    swprintf_s(protocol,_countof(protocol),L"CREO_OP PN_BUTTON %ls SINGLE SURFACE:%d CSYS:WORK_AT PLANE:RIGHT 70 54 2 6 0 0",name,face);
    recovery_check(recovery_action(model,protocol)&&!smart_active&&smart_steps==1&&smart_progress==1,
        "standalone panel operation retains one-response behavior");

done:
    smart_cancel();
    FILE *f=fopen("recovery-native.log","a");
    if(f){fprintf(f,"RECOVERY_CASES: %d\nRECOVERY_FAILURES: %d\nRECOVERY_DONE: 1\n",checks,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    uiCmdCmdId command;
    if((error=ProCmdActionAdd("BWTLRecoveryLiveTick",live_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command)))return error;
    error=ProCmdActionAdd("BWTLRecoveryProbe",recovery_fixture,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLRecoveryProbe`;");
}
