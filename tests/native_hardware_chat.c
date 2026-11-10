#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int failures,cases,tick_queued;
static ULONGLONG started;
static ProMdl test_root;
static ProAsmcomp base,target,duplicate;
static ProMatrix duplicate_pose;
static double mm;

static void check(int pass,const char *label){
    cases++;if(!pass)failures++;
    FILE *f=fopen("hardware-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int finish(void){
    KillTimer(native_backend_window,5432);
    smart_cancel();
    FILE *f=fopen("hardware-chat.log","a");
    if(f){fprintf(f,"CHAT_CASES: %d\nCHAT_FAILURES: %d\nCHAT_DONE: 1\n",cases,failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static void CALLBACK placement_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!tick_queued)tick_queued=ProMacroLoad(L"~ Command `BWTLHardwareChatTick`;")==PRO_TK_NO_ERROR;
}

static int placement_tick(uiCmdCmdId command,uiCmdValue *value,void *data){
    tick_queued=0;
    if(smart_active||ai_process){
        if(GetTickCount64()-started<260000)return 0;
        check(0,"real Codex hardware action timed out");
        return finish();
    }
    ProMatrix pose,unchanged;
    ProMdl model=NULL;
    AxScan *scan=NULL;
    int placed[2]={0};
    check(smart_intent==SMART_INTENT_CURRENT,"array request is classified as a current assembly write");
    check(wcsstr(smart_details,L"ASM_ARRAY_LINEAR ")&&smart_progress==1,"real Codex executed the native linear placement command");
    if(!ProAsmcompMdlGet(&target,&model)&&!ax_scan(test_root,1,&scan)){
        for(int i=0;i<scan->count;i++){
            AxOccurrence *entry=&scan->occurrences[i];
            ProAsmcomp component;
            if(entry->depth!=1||entry->model!=model||ax_component_feature(test_root,entry->ids[0],&component)||
               ax_actual_pose(test_root,&component,pose)||fabs(pose[3][1]*mm-5)>0.001||
               fabs(pose[3][2]*mm-30)>0.001)continue;
            for(int j=0;j<2;j++)if(fabs(pose[3][0]*mm-(65+45*j))<0.001)placed[j]++;
        }
    }
    free(scan);
    check(placed[0]==1&&placed[1]==1&&!part_health(test_root),"two copies at the exact requested pitch");
    ProMdl current=NULL;
    check(!ProMdlCurrentGet(&current)&&current==test_root&&!ax_actual_pose(test_root,&duplicate,unchanged)&&
        ax_same_pose(unchanged,duplicate_pose,mm),"top-level window and another instance remain in place");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"2 个独立实例")&&wcsstr(answer,L"未保存"),"one final result card");
    return finish();
}

static int placement_start(uiCmdCmdId command,uiCmdValue *value,void *data){
    ProMdl part=NULL;
    ProMdlName name=L"CHAT_BRACKET",root_name=L"CHAT_PLACEMENT";
    ModelPlan stock={0,60,40,10,0,1};
    ProMatrix at_base={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    ProMatrix at_target={{1,0,0,0},{0,1,0,0},{0,0,1,0},{20,5,30,1}};
    ProMatrix at_duplicate={{1,0,0,0},{0,1,0,0},{0,0,1,0},{90,15,40,1}};
    const char *stage="placement chat fixture";
    int window;
    ProError error;
    model_background_mode=0;
    if((error=quick_seed(name,&part))||
       (error=model_build_internal(&stock,name,&stage,part,NULL,NULL))||
       (error=model_create_empty(3,root_name,&stage))||
       (error=ProMdlnameInit(root_name,PRO_MDL_ASSEMBLY,&test_root))||
       (error=ProMdlWindowGet(test_root,&window))||(error=ProWindowCurrentSet(window))||
       (error=qa_mm_per_unit(test_root,&mm))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,at_base,&base))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,at_target,&target))||
       (error=ProAsmcompAssemble((ProAssembly)test_root,(ProSolid)part,at_duplicate,&duplicate))||
       (error=part_regenerate(test_root))||(error=ax_actual_pose(test_root,&duplicate,duplicate_pose))){
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
    swprintf_s(request,_countof(request),L"在当前装配里把 ID:%d 的支架沿总装 X 轴做等距直线组件阵列，总共 3 个（包括原件），间距 45 毫米。不保存。",target.id);
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
    error=ProCmdActionAdd("BWTLHardwareChatStart",placement_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLHardwareChatTick",placement_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLHardwareChatStart`;");
}
