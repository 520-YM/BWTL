#define BWTL_REUSE_EXPANSION_PATTERN
#include "native_expansion_pattern.c"

static ProMdl learned_source;
static ULONGLONG learned_start;
static int learned_queued;

static int finish_learning_chat(int passed){
    KillTimer(native_backend_window,9021);
    FILE *log=fopen("model-learning-chat.log","a");
    if(log){fprintf(log,"LEARNED_CHAT_PASS=%d\n",passed);fclose(log);}
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int learning_chat_tick(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    learned_queued=0;
    if(GetTickCount64()-learned_start>210000)return finish_learning_chat(0);
    if(smart_active||ai_process)return 0;
    ProMdl result=NULL;
    ProMassProperty actual;
    int accepted=!ProMdlCurrentGet(&result)&&result!=learned_source&&
        !qa_geometry_mass(result,&actual)&&fabs(actual.volume-(80*60*20-4*20*AS_PI*16))<0.001;
    const wchar_t *reply=wcsstr(history,L"【AI】");
    FILE *log=fopen("model-learning-chat.log","a");
    if(log){
        fprintf(log,"NEW_MODEL=%d NATIVE_STEPS=%d MODEL_WRITES=%d SINGLE_REPLY=%d\n",
            accepted,smart_steps,smart_progress,reply&&!wcsstr(reply+4,L"【AI】"));
        fclose(log);
    }
    return finish_learning_chat(accepted&&smart_progress>=3&&reply&&!wcsstr(reply+4,L"【AI】"));
}

static void CALLBACK learning_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    if(!learned_queued)learned_queued=ProMacroLoad(L"~ Command `BWTLLearningChatTick`;")==PRO_TK_NO_ERROR;
}

static int learning_chat_start(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    FILE *log=_wfopen(L"model-learning-chat-setup.log",L"wt, ccs=UTF-8");
    if(!log)return finish_learning_chat(0);
    model_background_mode=1;
    ProMdlName name;
    Fixture geometry;
    ProError error=fixture(&learned_source,name,&geometry);
    wchar_t command[512],report[PART_TEXT_MAX]=L"";
    PartText receipt={report,_countof(report),0};
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP HOLE %ls SEED SURFACE:%d SURFACE:%d 25 SURFACE:%d 20 6 THROUGH 0",
            name,geometry.top,geometry.x,geometry.y);
        error=expansion_build(learned_source,command,log);
    }
    if(!error){
        swprintf_s(command,_countof(command),L"CREO_OP FEATURE_PATTERN_LINEAR %ls DIR_PATTERN SEED PLANE:RIGHT 3 15 POS",name);
        error=expansion_build(learned_source,command,log);
    }
    ProPath delivered=L"";
    if(!error)error=imitation_run_mode(learned_source,delivered,1,0,&receipt);
    if(!error)error=method_save_last(&receipt);
    fwprintf(log,L"SETUP=%d\n%ls\n",error,report);
    fclose(log);
    if(error||!native_ui_open_threaded(NULL)||!SetTimer(native_backend_window,9021,1000,learning_chat_timer))
        return finish_learning_chat(0);
    memset(&method_last,0,sizeof(method_last));
    model_background_mode=0;
    learned_start=GetTickCount64();
    native_prompt_set(L"请按你学过的建模方法，新建一个80×60×20毫米的长方体实体零件。"
        L"在顶面做一排4个Ø8毫米通孔，种子孔距两侧基准分别25和20毫米，沿80毫米长度方向每隔10毫米。"
        L"尺寸齐全，直接完成建模，未要求保存。");
    run_local(NULL,NULL,NULL);
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError error=expansion_pattern_initialize(argc,argv,version,build,buffer);
    wchar_t codex[MAX_PATH];
    if(!error&&(!ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",codex,_countof(codex))||
       !runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)))
        error=PRO_TK_E_NOT_FOUND;
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLLearningChatStart",learning_chat_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLLearningChatTick",learning_chat_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLLearningChatStart`;");
}