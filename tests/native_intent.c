#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int checks,failures,tick_queued;
static ULONGLONG started;
#ifdef BWTL_NATIVE_CHAIR
static const wchar_t spoken_request[]=L"画个凳子，尺寸自己给。自己想象。加个靠背";
enum { expected_kind=Q_CHAIR };
#else
static const wchar_t spoken_request[]=L"画个凳子，尺寸自己给，自己想象。";
enum { expected_kind=Q_STOOL };
#endif

static void intent_check(int pass,const char *what){
    checks++;if(!pass)failures++;
    FILE *f=fopen("intent-native.log","a");
    if(f){fprintf(f,"%s: %d\n",what,pass?0:-1);fclose(f);}
}

static int intent_finish(void){
    KillTimer(native_backend_window,5402);
    smart_cancel();
    FILE *f=fopen("intent-native.log","a");
    if(f){fprintf(f,"INTENT_CASES: %d\nINTENT_FAILURES: %d\nINTENT_DONE: 1\n",checks,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static void CALLBACK intent_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!tick_queued)tick_queued=ProMacroLoad(L"~ Command `BWTLIntentTick`;")==PRO_TK_NO_ERROR;
}

static int intent_tick(uiCmdCmdId id,uiCmdValue *value,void *data){
    tick_queued=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-started<190000)return 0;
        intent_check(0,"real Codex request timed out");
        return intent_finish();
    }
    ProMdl model=NULL;
    ProFeature legs,back;
    ProMdlName name;
    ProOutline bounds;
    ProMassProperty mass;
    ProSolidBody body;
    int shape=!ProMdlCurrentGet(&model)&&!qa_single_body(model,&body)&&!part_health(model)&&
        !ux_named_feature(model,L"QP_STOOL_LEGS",&legs)&&!ProSolidBodyOutlineGet(&body,bounds)&&
        !qa_geometry_mass(model,&mass)&&!ProMdlMdlnameGet(model,name);
    if(shape&&expected_kind==Q_CHAIR)shape=!ux_named_feature(model,L"BWTL_STOOL_BACK",&back);
    intent_check(shape,"real spoken request creates one healthy solid with the requested native features");
    if(shape){
        FdTxn view={0};
        double mm=0;
        ProError error=qa_mm_per_unit(model,&mm);
        view.model=model;view.scale=error?1:1/mm;
        double top=bounds[1][2]*mm,h=-bounds[0][2]*mm;
        double w=(bounds[1][0]-bounds[0][0])*mm,d=(bounds[1][1]-bounds[0][1])*mm;
        PartCommand cmd;
        QuickPlan plan;
        wchar_t receipt[1800],line[1200];
        const wchar_t *row=NULL;
        if(store_read(L"last-native-details.txt",receipt,_countof(receipt)))row=wcsstr(receipt,expected_kind==Q_CHAIR?L"QUICK_CHAIR ":L"QUICK_STOOL ");
        const wchar_t *end=row?wcschr(row,L'｜'):NULL;
        int parsed=end&&swprintf_s(line,_countof(line),L"CREO_OP %.*ls",(int)(end-row),row)>0&&
            part_command_parse(line,&cmd)==1&&quick_plan(cmd.count,cmd.arg,&plan)==1;
        double t=parsed?plan.thickness:0;
        double expected=parsed?plan.width*plan.depth*t+4*plan.leg_width*plan.leg_width*h+
            plan.back_width*plan.back_thickness*plan.back_height:0;
        intent_check(!error&&parsed&&fabs(mass.volume*mm*mm*mm-expected)<0.1&&
            fabs(t+h-plan.height)<0.001&&fabs(top-t-plan.back_height)<0.001&&h>0,
            "actual height and volume include all requested features");
        double front_half=(d-(parsed?plan.back_thickness:0))/2;
        intent_check(!error&&parsed&&!quick_stool_ray(&view,w/2,front_half,h,t,0),"seat center has open space beneath");
        int feet=parsed&&!error;
        if(feet){
            double offset=plan.leg_inset+plan.leg_width/2;
            for(int i=0;i<4;i++){
                double x=i&1?w-offset:offset,y=i&2?d-offset:offset;
                double rise=plan.back_width>0&&fabs(x-w/2)<plan.back_width/2&&y>d-plan.back_thickness?plan.back_height:0;
                if(quick_stool_ray(&view,x,y,h,t+rise,1))feet=0;
            }
        }
        intent_check(feet,"four individual legs connect to the seat");
        ProPath directory,pattern;
        WIN32_FIND_DATAW data;
        int absent=0;
        if(!ProDirectoryCurrentGet(directory)&&swprintf_s(pattern,_countof(pattern),L"%ls\\%ls.prt*",directory,name)>0){
            HANDLE search=FindFirstFileW(pattern,&data);
            if(search==INVALID_HANDLE_VALUE)absent=GetLastError()==ERROR_FILE_NOT_FOUND;
            else FindClose(search);
        }
        intent_check(absent,"new stool was not automatically saved");
    }
    const wchar_t *answer=wcsstr(history,L"【AI】");
    intent_check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"四条")&&wcsstr(answer,L"未保存")&&
        (expected_kind!=Q_CHAIR||wcsstr(answer,L"直板靠背")),
        "one result card reports the verified stool");
    return intent_finish();
}

static int intent_start(uiCmdCmdId id,uiCmdValue *value,void *data){
    ProMdl model=NULL;
    ProMdlCurrentGet(&model);
    intent_check(!model,"start from a blank Creo session");
    if(model)return intent_finish();
    wchar_t codex[MAX_PATH];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)){
        intent_check(0,"start the private real Codex fixture");return intent_finish();
    }
    smart_begin(L"画个凳子，先给方案，别建模。");
    int denied=part_command_parse(L"CREO_OP QUICK_STOOL BLOCKED_STOOL 320 320 25 450 35 25",&pending_part)==1&&!smart_protocol_allowed(&pending_part);
    execute_part_command();
    ProMdlCurrentGet(&model);
    intent_check(denied&&!model&&!smart_active,"a request for discussion cannot execute the stool command");
    history[0]=0;conversation[0]=0;
    started=GetTickCount64();
    if(!SetTimer(native_backend_window,5402,1000,intent_timer)){
        intent_check(0,"arm private intent test timer");return intent_finish();
    }
    native_prompt_set(spoken_request);
    run_local(NULL,NULL,NULL);
    intent_check(smart_active&&smart_intent==SMART_INTENT_PENDING&&!request_requires_current&&quick_topic(smart_goal)==expected_kind,
        "exact informal request routes to new native modeling with chosen dimensions");
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    uiCmdCmdId command;
    error=ProCmdActionAdd("BWTLIntentTick",intent_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    if(!error)error=ProCmdActionAdd("BWTLIntentStart",intent_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command `BWTLIntentStart`;");
}
