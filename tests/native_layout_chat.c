#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int failures,cases,tick_queued;
static ULONGLONG started;
static ProMdl test_root;
static ProAsmcomp module;
static int skeleton_id;
static double mm;

static void check(int pass,const char *label){
    cases++;if(!pass)failures++;
    FILE *f=fopen("layout-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int finish(void){
    KillTimer(native_backend_window,5432);
    smart_cancel();
    FILE *f=fopen("layout-chat.log","a");
    if(f){fprintf(f,"CHAT_CASES: %d\nCHAT_FAILURES: %d\nCHAT_DONE: 1\n",cases,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static void CALLBACK placement_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!tick_queued)tick_queued=ProMacroLoad(L"~ Command `BWTLLayoutChatTick`;")==PRO_TK_NO_ERROR;
}

static int placement_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    tick_queued=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-started<260000)return 0;
        check(0,"real Codex enclosure action timed out");
        return finish();
    }
    ProMatrix pose;
    check(smart_intent==SMART_INTENT_CURRENT,"layout change uses current assembly write scope");
    check(wcsstr(smart_details,L"ASM_LAYOUT_DIM ")&&smart_progress==1,"real Codex executed one native layout dimension command");
    check(wcsstr(smart_details,L"定位发生变化 1 个实例"),"native receipt lists linked position");
    check(!ax_actual_pose(test_root,&module,pose)&&fabs(pose[3][0]*mm-140)<1e-5,"linked module actually moves to X140");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"140"),"one result card with updated size");
    return finish();
}

static ProError run_setup(ProMdl assembly,const wchar_t *text){
    PartCommand cmd;
    wchar_t report[PART_TEXT_MAX]=L"";
    if(part_command_parse(text,&cmd)!=1)return PRO_TK_BAD_INPUTS;
    return ax_execute(assembly,&cmd,report,_countof(report));
}

static int placement_start(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl part=NULL;
    ProMdlName name=L"CHAT_LAYOUT_PART",root_name=L"CHAT_LAYOUT";
    ModelPlan stock={0,20,15,5,0,1};
    ProMatrix at_origin={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const char *stage="layout chat fixture";
    int window;
    ProError error=PRO_TK_BAD_INPUTS;
    wchar_t setup[512];
    model_background_mode=0;
    if((error=quick_seed(name,&part))||(error=model_build_internal(&stock,name,&stage,part,NULL,NULL))||
       (error=model_create_empty(3,root_name,&stage))||(error=ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&test_root))||
       (error=ProMdlWindowGet(test_root,&window))||(error=ProWindowCurrentSet(window))||(error=qa_mm_per_unit(test_root,&mm))){
        trace(stage,error);check(0,"native assembly fixture created");return finish();
    }
    swprintf_s(setup,_countof(setup),L"CREO_OP ASM_LAYOUT_SKELETON %ls CHAT_SKEL 100 80 120",root_name);
    if((error=run_setup(test_root,setup))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,at_origin,&module))){
        trace(stage,error);check(0,"native assembly fixture created");return finish();
    }
    AxScan *scan=NULL;
    if(!ax_scan(test_root,0,&scan)){
        for(int i=0;i<scan->count;i++)if(!_wcsicmp(scan->occurrences[i].name,L"CHAT_SKEL"))skeleton_id=scan->occurrences[i].ids[0];
        free(scan);
    }
    swprintf_s(setup,_countof(setup),L"CREO_OP ASM_LAYOUT_LINK %ls ID:%d ID:%d CSYS:BWTL_WIDTH CSYS:PRT_CSYS_DEF",
        root_name,skeleton_id,module.id);
    if(!skeleton_id||(error=run_setup(test_root,setup))){
        trace(stage,error);check(0,"native layout fixture linked");return finish();
    }
    ProError queued=ProMacroLoad(L"~ Command `BWTLLayoutChatRequest`;");
    if(queued){check(0,"queue layout chat after assembly returns to Creo");return finish();}
    return 0;
}

static int request_layout(uiCmdCmdId command,uiCmdValue *value,void *data){
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
    swprintf_s(request,_countof(request),L"在当前装配执行第297项。修改骨架 ID:%d 的控制尺寸 BW_WIDTH，确认旧值是100毫米后改成140毫米，并核对哪些组件实际联动。直接执行，模型不保存。",skeleton_id);
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
    error=ProCmdActionAdd("BWTLLayoutChatStart",placement_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLayoutChatTick",placement_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLayoutChatRequest",request_layout,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLLayoutChatStart`;");
}
