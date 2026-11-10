#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int failures,cases,tick_queued;
static ULONGLONG started;
static ProMdl test_root;
static ProAsmcomp moving,obstacle;
static ProWVerstamp original;
static double mm;

static void check(int pass,const char *label){
    cases++;if(!pass)failures++;
    FILE *f=fopen("mechanism-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int finish(void){
    KillTimer(native_backend_window,5432);
    smart_cancel();
    FILE *f=fopen("mechanism-chat.log","a");
    if(f){fprintf(f,"CHAT_CASES: %d\nCHAT_FAILURES: %d\nCHAT_DONE: 1\n",cases,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static void CALLBACK placement_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!tick_queued)tick_queued=ProMacroLoad(L"~ Command `BWTLMechanismChatTick`;")==PRO_TK_NO_ERROR;
}

static int placement_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    tick_queued=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-started<260000)return 0;
        check(0,"real Codex enclosure action timed out");
        return finish();
    }
    ProWVerstamp after=NULL;
    ProMdl current=NULL;
    check(smart_intent==SMART_INTENT_READ,"space check uses read scope");
    check(wcsstr(smart_details,L"ASM_TRAY_SWEEP ")&&smart_steps>=1,"real Codex used the 289 native inspection");
    check(wcsstr(smart_details,L"首次采样干涉位置 8 mm")&&wcsstr(smart_details,L"干涉 3 个"),
        "native receipt reports known interferences");
    check(!ProMdlCurrentGet(&current)&&current==test_root&&!ProMdlVerstampGet(test_root,&after)&&
        ProVerstampEqual(original,after),"source assembly and active window unchanged");
    if(after)ProVerstampFree(&after);
    if(original)ProVerstampFree(&original);
    const wchar_t *answer=wcsstr(history,L"【AI】");
    check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"干涉"),"one result card with interference report");
    return finish();
}

static int placement_start(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl part=NULL;
    ProMdlName name=L"CHAT_TRAY",root_name=L"CHAT_MOTION";
    ModelPlan stock={0,20,15,5,0,1};
    ProMatrix start={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProMatrix end={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,12,1}};
    PartCommand axis;
    wchar_t report[PART_TEXT_MAX]=L"";
    const char *stage="motion chat fixture";
    int window;
    ProError error=PRO_TK_BAD_INPUTS;
    model_background_mode=0;
    if(part_command_parse(L"CREO_OP DATUM_AXIS CHAT_TRAY PIN PLANES PLANE:RIGHT PLANE:TOP",&axis)!=1||
       (error=quick_seed(name,&part))||
       (error=model_build_internal(&stock,name,&stage,part,NULL,NULL))||
       (error=foundation_execute(&axis,part,report,_countof(report)))||
       (error=model_create_empty(3,root_name,&stage))||
       (error=ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&test_root))||
       (error=ProMdlWindowGet(test_root,&window))||(error=ProWindowCurrentSet(window))||
       (error=qa_mm_per_unit(test_root,&mm))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,start,&moving))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,end,&obstacle))||
       (error=part_regenerate(test_root))||(error=ProMdlVerstampGet(test_root,&original))){
        trace(stage,error);check(0,"native assembly fixture created");return finish();
    }
    check(1,"native assembly fixture created");
    wchar_t exe[MAX_PATH],request[600];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
       L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
       !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)){
        check(0,"start private real Codex");return finish();
    }
    if(!SetTimer(native_backend_window,5432,1000,placement_timer)){
        check(0,"arm placement fixture");return finish();
    }
    swprintf_s(request,_countof(request),L"用第289项检查当前装配的托盘抽出空间。移动件 ID:%d，障碍件 ID:%d，沿移动件 AXIS:PIN 正向抽出20毫米，包含起点和终点共6个采样，最小间隙要求1毫米。只检查，告诉我干涉和第一个干涉位置。",moving.id,obstacle.id);
    started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    if(error)return error;
    uiCmdCmdId id;
    error=ProCmdActionAdd("BWTLMechanismChatStart",placement_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLMechanismChatTick",placement_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLMechanismChatStart`;");
}
