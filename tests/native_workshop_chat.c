#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static ULONGLONG workshop_chat_started;

static int workshop_chat_finish(int success){
    FILE *log=fopen("workshop-chat.log","a");
    if(log){fprintf(log,"WORKSHOP_CHAT_PASS: %d\n",success);fclose(log);}
    if(native_backend_window)KillTimer(native_backend_window,5460);
    smart_cancel();
    ProEngineerEnd();
    return 0;
}

static int workshop_chat_verify(uiCmdCmdId id,uiCmdValue *value,void *context){
    if(smart_active||ai_process)
        return GetTickCount64()-workshop_chat_started<80000?0:workshop_chat_finish(0);
    const wchar_t *answer=wcsstr(history,L"【AI】");
    return workshop_chat_finish(smart_steps==1&&wcsstr(smart_details,L"SMY_STOCK_MEASURE ")&&
        answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"4800"));
}

static void CALLBACK workshop_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    ProMacroLoad(L"~ Command \140BWTLWorkshopChatVerify\140;");
}

static ProMdl workshop_chat_model;
static int workshop_chat_face;

static int workshop_chat_send(uiCmdCmdId id,uiCmdValue *value,void *context){
    wchar_t exe[MAX_PATH],request[512];
    ProMdl current=NULL;
    if(ProMdlCurrentGet(&current)||current!=workshop_chat_model||!native_ui_open_threaded(NULL)||
       !ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
       !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
       !SetTimer(native_backend_window,5460,1000,workshop_chat_timer))return workshop_chat_finish(0);
    swprintf_s(request,_countof(request),L"请执行钣金车间第451项，以 SURFACE:%d 为展开固定面，使用密度 7850 kg/m³，测量当前钣金零件的净料面积、毛坯面积和单件重量。直接在聊天给结果。",workshop_chat_face);
    workshop_chat_started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int workshop_chat_start(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProMdlName name;
    const char *stage="create workshop chat fixture";
    ProMdl model=NULL;
    ProFeature wall;
    CabinetFace face={0};
    model_background_mode=0;
    ProError error=model_create_empty(2,name,&stage);
    if(!error)error=ProMdlnameInit(name,PRO_MDL_PART,&model);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_SKETCH %ls OUTLINE PLANE:FRONT PLANE:RIGHT RECT 0 0 80 60",name);
    if(!error)error=cm_native_command(model,L"CREO_OP SMT_FIRST_WALL %ls BASE SKETCH:OUTLINE 2 POS",name);
    if(!error)error=dv_display_new(model);
    if(!error)error=cm_feature_find(model,L"BASE",&wall);
    if(!error)error=cm_wall_face(&wall,&face);
    if(error||!face.id)return workshop_chat_finish(0);
    workshop_chat_model=model;
    workshop_chat_face=face.id;
    return ProMacroLoad(L"~ Command \140BWTLWorkshopChatSend\140;");
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t error_buffer[80]){
    ProError error=bwtl_production_initialize(argc,argv,version,build,error_buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLWorkshopChatStart",workshop_chat_start,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLWorkshopChatSend",workshop_chat_send,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLWorkshopChatVerify",workshop_chat_verify,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command \140BWTLWorkshopChatStart\140;");
}

