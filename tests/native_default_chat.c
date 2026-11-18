#define user_initialize production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl source_part;
static SmartPartSignature source_before;
static FILE *checks;
static int phase,queued,failures;
static ULONGLONG deadline;

static void result(const char *name,int pass){
    fprintf(checks,"%s=%d\n",name,pass?1:0);
    fflush(checks);
    if(!pass)failures++;
}

static int finish(void){
    KillTimer(native_backend_window,6067);

    fprintf(checks,"DEFAULT_CHAT_FINAL=%d\n",failures);
    fclose(checks);
    ProEngineerEnd();
    return 0;
}

static int tick(uiCmdCmdId id,uiCmdValue *value,void *data);
static void CALLBACK timer(HWND hwnd,UINT msg,UINT_PTR id,DWORD clock){
    if(phase){tick(0,NULL,NULL);return;}
    if(!queued)queued=!ProMacroLoad(L"~ Command \x0060" L"BWTLDefaultChatTick\x0060;");
}

static int tick(uiCmdCmdId id,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()>deadline){result("TIMELY_RESULT",0);return finish();}
    ProMdl active=NULL;
    if(ProMdlCurrentGet(&active))return 0;
    if(!phase){
        if(active!=source_part)return 0;
        native_prompt_set(L"创建一个空装配，把这个摄像头支架默认装配到里面。");
        run_local(NULL,NULL,NULL);
        phase=1;
        return 0;
    }
    if(smart_active||ai_process)return 0;
    AxScan *scan=NULL;
    ProMdlType type;
    int placed=active!=source_part&&!ProMdlTypeGet(active,&type)&&type==PRO_MDL_ASSEMBLY&&
        !ax_scan(active,1,&scan)&&scan->count==1&&scan->occurrences[0].model==source_part;
    result("IMPLEMENTED_TWO_REQUESTED_STEPS",placed);
    if(placed){
        ProAsmcomp component;
        result("NATIVE_DEFAULT",!ax_component_feature(active,scan->occurrences[0].ids[0],&component)&&
            !ax_default_verify(&component));
    }
    free(scan);
    SmartPartSignature after=smart_part_signature(source_part);
    result("ORIGINAL_PART_PRESERVED",!after.error&&after.hash==source_before.hash);
    const wchar_t *answer=wcsstr(history,L"【AI】");
    result("SINGLE_VERIFIED_REPLY",answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"DEFAULT_CHAT_BRACKET")&&
        wcsstr(answer,L"默认定位")&&!wcsstr(answer,L"ASM_W_FIND"));
    return finish();
}

static int start_check(uiCmdCmdId id,uiCmdValue *value,void *data){
    checks=fopen("default-assembly-chat.log","w");
    if(!checks){ProEngineerEnd();return 0;}
    result("COMBINED_INTENT",assembly_default_new_request(L"创建一个空装配，把这个摄像头支架默认装配到里面。"));
    result("QUESTION_RETAINS_NORMAL_CHAT",!assembly_default_new_request(L"创建一个空装配，把这个支架默认装配到里面可以吗？"));
    ProMdlName name=L"DEFAULT_CHAT_BRACKET";
    ModelPlan plan={.a=80,.b=60,.height=6,.ready=1};
    const char *stage;
    ProError e=quick_seed(name,&source_part);
    if(!e)e=model_build_internal(&plan,name,&stage,source_part,NULL,NULL);
    if(!e){source_before=smart_part_signature(source_part);e=source_before.error;}
    if(!e)e=dv_display_new(source_part);
    result("CREATED_IN_SESSION_SOURCE",!e);
    if(e||!native_ui_open_threaded(NULL)||!SetTimer(native_backend_window,6067,400,timer)){result("READY",0);return finish();}
    deadline=GetTickCount64()+16000;
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    ProError e=production_init(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!e)e=ProCmdActionAdd("BWTLDefaultChatStart",start_check,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!e)e=ProCmdActionAdd("BWTLDefaultChatTick",tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return e?e:ProMacroLoad(L"~ Command \x0060" L"BWTLDefaultChatStart\x0060;");
}