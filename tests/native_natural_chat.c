#define user_initialize production_chat_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ProMdl original;
static ProWVerstamp original_stamp;
static FILE *results;
static ULONGLONG start;
static int phase,queued,source_window;
static int failures;

static void check(const char *label,int passed){
    fprintf(results,"%s=%d\n",label,passed);
    fflush(results);
    if(!passed)failures++;
}

static int finish_chat_test(void){
    if(native_backend_window)KillTimer(native_backend_window,9020);
    if(original_stamp)ProVerstampFree(&original_stamp);
    fprintf(results,"FAILURES=%d\nDONE=1\n",failures);
    fclose(results);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static void send_from_window(const wchar_t *text){
    history[0]=0;
    native_prompt_set(text);
    run_local(NULL,NULL,NULL);
}

static int single_reply(void){
    const wchar_t *reply=wcsstr(history,L"【AI】");
    return reply&&!wcsstr(reply+4,L"【AI】")&&!wcsstr(reply,L"本轮仅执行你要求的查看或分析");
}

static void verify_model(const char *label){
    ProMdl result=NULL;
    ProMdlsubtype type=0;
    double size[3]={0},mm=0,thickness=0;
    ImitationNativeCheck features={0};
    ProDimension hole;
    double diameter=0;
    ProWVerstamp stamp=NULL;
    int same_source=!ProMdlVerstampGet(original,&stamp)&&ProVerstampEqual(original_stamp,stamp);
    if(stamp)ProVerstampFree(&stamp);
    int success=!ProMdlCurrentGet(&result)&&result!=original&&
        !ProMdlSubtypeGet(result,&type)&&type==PROMDLSTYPE_PART_SHEETMETAL&&
        !fab_thickness(result,&mm,&thickness)&&fabs(thickness-1)<.002&&
        !imitation_extents(result,mm,size)&&fabs(size[0]-600)<.002&&
        fabs(size[1]-200)<.002&&fabs(size[2]-300)<.002;
    if(success){
        features.result=result;
        success=!ProSolidFeatVisit((ProSolid)original,imitation_check_feature,NULL,&features)&&
            features.features>=8&&!ProModelitemInit(result,28,PRO_DIMENSION,&hole)&&
            !ProDimensionValueGet(&hole,&diameter)&&fabs(diameter*mm-100)<.002;
    }
    fprintf(results,"size=%.8g %.8g %.8g, gauge=%.8g, hole=%.8g, features=%d\n",
            size[0],size[1],size[2],thickness,diameter*mm,features.features);
    check(label,success&&same_source&&single_reply()&&smart_progress==1);
}

static int chat_tick(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    queued=0;
    if(GetTickCount64()-start>240000){check("TIMEOUT",0);return finish_chat_test();}
    if(smart_active||ai_process)return 0;
    if(phase==1){
        ProMdl active=NULL;
        check("NATURAL_CHAT",smart_intent==SMART_INTENT_CHAT&&single_reply()&&smart_steps==0&&
              !ProMdlCurrentGet(&active)&&active==original);
        phase=2;
        send_from_window(L"你现在仿照这个222，画一个600*200*300的模型。");
    }else if(phase==2){
        verify_model("EXPLICIT_IMITATION");
        if(ProWindowActivate(source_window)){check("RETURN_TO_SOURCE",0);return finish_chat_test();}
        phase=3;
        wcscpy_s(conversation,_countof(conversation),
            L"用户：你现在仿照这个222，画一个600*200*300的模型。\n"
            L"助手：本轮按你的分析或查看要求处理，模型保持原样。\n");
    }else if(phase==3){
        ProMdl active=NULL;
        if(ProMdlCurrentGet(&active)||active!=original)return 0;
        phase=4;
        send_from_window(L"叫你画一个，就是叫你执行，而不是叫你分析。");
    }else if(phase==4){
        verify_model("CONTEXTUAL_CORRECTION");
        return finish_chat_test();
    }
    return 0;
}

static void CALLBACK chat_timer(HWND window,UINT msg,UINT_PTR id,DWORD now){
    if(!queued)queued=ProMacroLoad(L"~ Command `BWTLNaturalChatTick`;")==PRO_TK_NO_ERROR;
}

static int chat_begin(uiCmdCmdId cmd,uiCmdValue *value,void *data){
    results=fopen("natural-chat-result.log","w");
    if(!results){ProEngineerEnd();return 0;}
    ProPath source;
    if(!GetEnvironmentVariableW(L"BWTL_FRAME_ORIGINAL",source,_countof(source))||
       ProMdlFiletypeLoad(source,PRO_MDLFILE_PART,PRO_B_FALSE,&original)||
       dv_display_new(original)||ProWindowCurrentGet(&source_window)||ProMdlVerstampGet(original,&original_stamp)){
        check("OPEN_SOURCE",0);return finish_chat_test();
    }
    native_ui_ready=CreateEventW(NULL,TRUE,FALSE,NULL);
    native_ui_thread=CreateThread(NULL,0,native_ui_thread_main,NULL,0,NULL);
    if(!native_ui_thread||WaitForSingleObject(native_ui_ready,5000)!=WAIT_OBJECT_0){
        check("CHAT_WINDOW",0);return finish_chat_test();
    }
    CloseHandle(native_ui_ready);
    native_ui_ready=NULL;
    if(!SetTimer(native_backend_window,9020,1000,chat_timer)){check("POLL_TIMER",0);return finish_chat_test();}
    start=GetTickCount64();
    phase=1;
    send_from_window(L"你好，我想跟你聊聊。你能怎么帮我讨论钣金设计的取舍？先聊设计思路。");
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    ProError e=production_chat_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId cmd;
    const wchar_t *codex=L"C:\\Users\\Administrator\\AppData\\Local\\Programs\\OpenAI\\Codex\\bin\\codex.exe";
    if(!e&&(!runtime_exe_exists(codex)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",codex,runtime_settings)))e=PRO_TK_E_NOT_FOUND;
    if(!e)e=ProCmdActionAdd("BWTLNaturalChatBegin",chat_begin,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    if(!e)e=ProCmdActionAdd("BWTLNaturalChatTick",chat_tick,uiProe2ndImmediate,access_command,PRO_B_TRUE,PRO_B_TRUE,&cmd);
    return e?e:ProMacroLoad(L"~ Command `BWTLNaturalChatBegin`;");
}
