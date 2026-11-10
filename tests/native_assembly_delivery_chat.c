
#define user_initialize bwtl_production_initialize
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int delivery_chat_failures,delivery_chat_cases;
static ULONGLONG delivery_chat_started;


static void delivery_chat_check(int pass,const char *label){
    delivery_chat_cases++;
    if(!pass)delivery_chat_failures++;
    FILE *f=fopen("delivery-chat.log","a");
    if(f){fprintf(f,"%s: %d\n",label,pass?0:-1);fclose(f);}
}

static int delivery_chat_done(void){
    if(native_backend_window)KillTimer(native_backend_window,5441);
    smart_cancel();
    FILE *f=fopen("delivery-chat.log","a");
    if(f){fprintf(f,"DELIVERY_CHAT_CASES: %d\nDELIVERY_CHAT_FAILURES: %d\nDELIVERY_CHAT_DONE: 1\n",
        delivery_chat_cases,delivery_chat_failures);fclose(f);}
    ProEngineerEnd();
    return 0;
}

static int delivery_chat_tick(uiCmdCmdId id,uiCmdValue *value,void *context){
    if(smart_active||ai_process){
        if(GetTickCount64()-delivery_chat_started<250000)return 0;
        delivery_chat_check(0,"native chat timed out");return delivery_chat_done();
    }
    delivery_chat_check(smart_intent==SMART_INTENT_READ,"metadata check uses read-only intent");
    delivery_chat_check(smart_steps==1&&wcsstr(smart_details,L"ASM_B_CHECK "),"native metadata check executed in one turn");
    const wchar_t *answer=wcsstr(history,L"【AI】");
    delivery_chat_check(answer&&!wcsstr(answer+4,L"【AI】")&&wcsstr(answer,L"缺材料")&&
        wcsstr(answer,L"BOM 资料完整性"),"one final reply with missing material");
    return delivery_chat_done();
}

static void CALLBACK delivery_chat_timer(HWND window,UINT message,UINT_PTR timer,DWORD now){
    ProMacroLoad(L"~ Command `BWTLDeliveryChatTick`;");
}

static int delivery_chat_request(uiCmdCmdId id,uiCmdValue *value,void *context){
    wchar_t exe[MAX_PATH],request[600];
    if(!native_ui_open_threaded(NULL)||!ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin\\codex.exe",exe,_countof(exe))||
        !runtime_exe_exists(exe)||!WritePrivateProfileStringW(L"Runtime",L"CodexPath",exe,runtime_settings)||
        !SetTimer(native_backend_window,5441,1000,delivery_chat_timer)){
        delivery_chat_check(0,"start private Codex quality chat");return delivery_chat_done();
    }
    wcscpy_s(request,_countof(request),L"请按第334项检查当前装配的 BOM 资料完整性，找出缺失料号、图号、材料或规格的实例。不导出，在聊天里直接给结果。");
    delivery_chat_started=GetTickCount64();
    native_prompt_set(request);
    run_local(NULL,NULL,NULL);
    return 0;
}

static int delivery_chat_prepare(uiCmdCmdId id,uiCmdValue *value,void *context){
    ProPath folder,root_leaf,work,pattern,source,target;
    ProMdl root=NULL;
    AxScan *scan=NULL;
    int window;
    ProError error=ProDirectoryCurrentGet(work);
    DWORD n=GetEnvironmentVariableW(L"BWTL_REOPEN_PACKAGE",folder,_countof(folder));
    DWORD m=GetEnvironmentVariableW(L"BWTL_REOPEN_ASSEMBLY",root_leaf,_countof(root_leaf));
    if(!n||n>=_countof(folder)||!m||m>=_countof(root_leaf))error=PRO_TK_BAD_INPUTS;
    if(!error&&swprintf_s(pattern,_countof(pattern),L"%ls\\*",folder)<0)error=PRO_TK_BAD_INPUTS;
    if(!error){
        WIN32_FIND_DATAW entry;
        HANDLE find=FindFirstFileW(pattern,&entry);
        int copied=0;
        if(find==INVALID_HANDLE_VALUE)error=PRO_TK_CANT_OPEN;
        else{
            do{
                if(entry.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
                if(!wcsstr(entry.cFileName,L".asm.")&&!wcsstr(entry.cFileName,L".prt."))continue;
                if(swprintf_s(source,_countof(source),L"%ls\\%ls",folder,entry.cFileName)<0||
                   swprintf_s(target,_countof(target),L"%ls\\%ls",work,entry.cFileName)<0||
                   !CopyFileW(source,target,TRUE)){error=PRO_TK_CANT_WRITE;break;}
                copied++;
            }while(FindNextFileW(find,&entry));
            FindClose(find);
        }
        if(!error&&copied!=4)error=PRO_TK_INCOMPLETE;
    }
    if(!error&&swprintf_s(target,_countof(target),L"%ls\\%ls",work,root_leaf)<0)error=PRO_TK_BAD_INPUTS;
    if(!error)error=ProMdlFiletypeLoad(target,PRO_MDLFILE_ASSEMBLY,PRO_B_FALSE,&root);
    if(!error)error=ax_scan(root,1,&scan);
    int package_local=0;
    if(!error&&scan->count==4&&scan->parts==3){
        package_local=1;
        size_t prefix=wcslen(work);
        for(int i=0;i<scan->count;i++){
            ProPath origin;
            ProMdl model=scan->occurrences[i].model;
            if(!model||ProMdlOriginGet(model,origin)||_wcsnicmp(origin,work,prefix)||
                (work[prefix-1]!=L'\\'&&origin[prefix]!=L'\\'))package_local=0;
        }
    }
    delivery_chat_check(package_local,"reopened native assembly with dependencies from package only");
    free(scan);
    ProMdlName name;
    if(!error)error=ProMdlMdlnameGet(root,name);
    if(!error)error=ProObjectwindowMdlnameCreate(name,PRO_ASSEMBLY,&window);
    if(!error)error=ProMdlWindowGet(root,&window);
    if(!error)error=ProWindowCurrentSet(window);
    if(!error)error=ProMdlDisplay(root);
    if(!error)error=ProWindowActivate(window);
    if(!error)error=ProMacroLoad(L"~ Command \140BWTLDeliveryChatRequest\140;");
    if(error){trace("assembly package reopen",error);delivery_chat_check(0,"reopen and chat fixture");return delivery_chat_done();}
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    char enabled[16];
    if(!GetEnvironmentVariableA("BWTL_FOUNDATION_SELFTEST",enabled,sizeof(enabled))||strcmp(enabled,"yes"))return PRO_TK_BAD_CONTEXT;
    ProError error=bwtl_production_initialize(argc,argv,version,build,buffer);
    uiCmdCmdId id;
    if(!error)error=ProCmdActionAdd("BWTLDeliveryChatPrepare",delivery_chat_prepare,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLDeliveryChatRequest",delivery_chat_request,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    if(!error)error=ProCmdActionAdd("BWTLDeliveryChatTick",delivery_chat_tick,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&id);
    return error?error:ProMacroLoad(L"~ Command `BWTLDeliveryChatPrepare`;");
}
