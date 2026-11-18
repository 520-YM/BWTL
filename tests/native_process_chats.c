#define user_initialize process_chat_production_init
#include "../src/assistant.c"
#undef user_initialize
#include <ProCore.h>

static int phase;
static int failures;
static FILE *receipt;

static void archive_check(int pass,const char *name){
    if(!pass)failures++;
    fprintf(receipt,"%s=%d\n",name,!!pass);
    fflush(receipt);
}

static int archive_checks(uiCmdCmdId command,uiCmdValue *value,void *data){
    wchar_t text[12000],path[MAX_PATH],first[MAX_PATH];
    char logname[100];
    sprintf_s(logname,sizeof(logname),"process-chats-%d.log",phase);
    receipt=fopen(logname,"w");
    if(!receipt){ProEngineerEnd();return 0;}
    archive_check(!conversation[0]&&!smart_details[0],"NEW_PROCESS_CONTEXT_EMPTY");
    archive_check(wcsstr(memory,L"PROCESS_CHAT_MEMORY_KEEP")!=NULL,"LONG_TERM_MEMORY_KEPT");
    archive_check(open_window(0,NULL,NULL)==0&&!history[0],"NEW_WINDOW_EMPTY");
    FILE *prompt=_wfopen(L"history-probe.txt",L"wt, ccs=UTF-8");
    archive_check(prompt!=NULL,"PROMPT_OPEN");
    if(prompt){
        history_context_write_mode(prompt,L"PROCESS_CHAT_OLD",0);
        fclose(prompt);
        prompt=_wfopen(L"history-probe.txt",L"rt, ccs=UTF-8");
        size_t read=prompt?fread(text,sizeof(wchar_t),_countof(text)-1,prompt):0;
        if(prompt)fclose(prompt);
        text[read]=0;
        archive_check(!wcsstr(text,L"PROCESS_CHAT_OLD"),"OLD_DIALOGUE_NOT_IN_NEW_PROMPT");
    }
    if(phase==1){
        swprintf_s(path,_countof(path),L"%ls\\历史记录_升级前\\chat-history.md",store_archive_root);
        FILE *legacy=_wfopen(path,L"rt, ccs=UTF-8");
        size_t n=legacy?fread(text,sizeof(wchar_t),_countof(text)-1,legacy):0;
        if(legacy)fclose(legacy);
        text[n]=0;
        archive_check(wcsstr(text,L"PROCESS_CHAT_OLD")!=NULL,"OLD_HISTORY_ARCHIVED");
        append_card(L"你",L"PROCESS_CHAT_FIRST_MESSAGE");
        save_turn(L"用户",L"PROCESS_CHAT_FIRST_MESSAGE");
        archive_check(store_write(L"first-native-session.txt",store_session_root),"RECORD_FIRST_ARCHIVE");
        on_closed(NULL,NULL,NULL);
        archive_check(open_window(0,NULL,NULL)==0&&wcsstr(history,L"PROCESS_CHAT_FIRST_MESSAGE")!=NULL,
            "REOPEN_SAME_PROCESS_KEEPS_CHAT");
        archive_check(store_read(L"chat-history.md",text,_countof(text))&&wcsstr(text,L"PROCESS_CHAT_FIRST_MESSAGE")!=NULL,
            "CURRENT_TURN_SAVED_IMMEDIATELY");
        archive_check(store_write(L"last-native-details.txt",L"PROCESS_CHAT_FIRST_DETAILS"),"CURRENT_DETAILS_SAVED");
        FILE *marker=fopen("process-chats-phase.txt","w");
        if(marker){fputs("2",marker);fclose(marker);}else failures++;
    }else{
        archive_check(store_read(L"first-native-session.txt",first,_countof(first))&&
            wcscmp(first,store_session_root)!=0,"SECOND_PROCESS_OWNS_NEW_ARCHIVE");
        swprintf_s(path,_countof(path),L"%ls\\chat-history.md",first);
        FILE *previous=_wfopen(path,L"rt, ccs=UTF-8");
        size_t n=previous?fread(text,sizeof(wchar_t),_countof(text)-1,previous):0;
        if(previous)fclose(previous);
        text[n]=0;
        archive_check(wcsstr(text,L"PROCESS_CHAT_FIRST_MESSAGE")!=NULL,"FIRST_PROCESS_CHAT_PRESERVED");
        append_card(L"你",L"PROCESS_CHAT_SECOND_MESSAGE");
        save_turn(L"用户",L"PROCESS_CHAT_SECOND_MESSAGE");
        archive_check(store_read(L"chat-history.md",text,_countof(text))&&wcsstr(text,L"PROCESS_CHAT_SECOND_MESSAGE")&&
            !wcsstr(text,L"PROCESS_CHAT_FIRST_MESSAGE"),"SECOND_PROCESS_SEPARATE_CONTENT");
    }
    archive_check(!wcsncmp(native_attachment_root,store_session_root,wcslen(store_session_root)),
        "ATTACHMENTS_SHARE_SESSION_FOLDER");
    fprintf(receipt,"PROCESS_CHATS_NATIVE_PASS=%d\n",!failures);
    fclose(receipt);
    ProEngineerEnd();
    return 0;
}

__declspec(dllexport) int user_initialize(int argc,char **argv,char *version,char *build,wchar_t buffer[80]){
    if(!GetEnvironmentVariableW(L"BWTL_FOUNDATION_SELFTEST",NULL,0))return PRO_TK_BAD_CONTEXT;
    phase=GetFileAttributesW(L"process-chats-phase.txt")==INVALID_FILE_ATTRIBUTES?1:2;
    if(phase==1){
        wchar_t profile[MAX_PATH],root[MAX_PATH];
        if(!GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,_countof(profile))||
           swprintf_s(root,_countof(root),L"%ls\\data",profile)<0||!store_init(root)||
           !store_write(L"recent-context.txt",L"PROCESS_CHAT_OLD_CONTEXT")||
           !store_write(L"last-native-details.txt",L"PROCESS_CHAT_OLD_DETAILS")||
           !store_write(L"memory.txt",L"PROCESS_CHAT_MEMORY_KEEP")||
           !store_log(L"用户",L"PROCESS_CHAT_OLD"))return PRO_TK_CANT_WRITE;
    }
    ProError error=process_chat_production_init(argc,argv,version,build,buffer);
    uiCmdCmdId command;
    if(!error)error=ProCmdActionAdd("BWTLProcessChatCheck",archive_checks,uiProe2ndImmediate,NULL,PRO_B_TRUE,PRO_B_TRUE,&command);
    return error?error:ProMacroLoad(L"~ Command \x0060" L"BWTLProcessChatCheck\x0060;");
}
