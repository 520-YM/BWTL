#define _CRT_SECURE_NO_WARNINGS
#include "../src/chat_store.h"
#include <assert.h>

static void path_for(wchar_t path[MAX_PATH],const wchar_t *folder,const wchar_t *name){
    assert(swprintf_s(path,MAX_PATH,L"%ls\\%ls",folder,name)>=0);
}

static void read_text(const wchar_t *path,wchar_t buffer[12000]){
    FILE *stream=_wfopen(path,L"rt, ccs=UTF-8");
    assert(stream);
    size_t count=fread(buffer,sizeof(wchar_t),11999,stream);
    assert(!ferror(stream));
    buffer[count]=0;
    assert(!fclose(stream));
}

int wmain(int argc,wchar_t **argv){
    wchar_t text[12000],path[MAX_PATH],other[MAX_PATH];
    assert(argc==3&&store_init(argv[2]));
    if(!wcscmp(argv[1],L"first")){
        assert(store_write(L"memory.txt",L"永久规则甲\n"));
        assert(store_write(L"recent-context.txt",L"旧版用户与助手记录\n"));
        assert(store_write(L"last-native-details.txt",L"旧版加工结果\n"));
        assert(store_log(L"用户",L"旧版聊天正文甲"));
        assert(store_session_begin()&&store_session_root[0]);
        wchar_t active[MAX_PATH];
        wcscpy_s(active,MAX_PATH,store_session_root);
        assert(store_session_begin()&&!wcscmp(active,store_session_root));
        assert(store_read(L"recent-context.txt",text,_countof(text))&&!text[0]);
        assert(store_read(L"last-native-details.txt",text,_countof(text))&&!text[0]);
        assert(store_read(L"memory.txt",text,_countof(text))&&wcsstr(text,L"永久规则甲"));
        path_for(path,store_archive_root,L"历史记录_升级前\\chat-history.md");
        read_text(path,text);assert(wcsstr(text,L"旧版聊天正文甲"));
        path_for(path,store_root,L"chat-history.md");
        read_text(path,text);assert(wcsstr(text,L"旧版聊天正文甲")&&!wcsstr(text,L"第一个Creo"));
        assert(store_log(L"用户",L"第一个Creo进程的提问"));
        assert(store_log(L"助手",L"第一个Creo进程的回答"));
        assert(store_write(L"recent-context.txt",L"第一个Creo的近期上下文"));
        assert(store_write(L"last-native-details.txt",L"第一个Creo的加工结果"));
        assert(store_write(L"first-session-path.txt",store_session_root));
        puts("FIRST PROCESS PASS: legacy backup, empty context and details, memory, immediate archived turns.");
    }else if(!wcscmp(argv[1],L"second")){
        assert(store_read(L"first-session-path.txt",text,_countof(text)));
        wchar_t first[MAX_PATH];wcscpy_s(first,MAX_PATH,text);
        assert(store_session_begin()&&wcscmp(first,store_session_root));
        assert(store_read(L"recent-context.txt",text,_countof(text))&&!text[0]);
        assert(store_read(L"last-native-details.txt",text,_countof(text))&&!text[0]);
        assert(store_read(L"memory.txt",text,_countof(text))&&wcsstr(text,L"永久规则甲"));
        path_for(path,first,L"chat-history.md");
        read_text(path,text);
        assert(wcsstr(text,L"第一个Creo进程的提问")&&wcsstr(text,L"第一个Creo进程的回答"));
        assert(store_log(L"用户",L"第二个Creo进程的提问"));
        assert(store_read(L"chat-history.md",text,_countof(text))&&wcsstr(text,L"第二个Creo")&&!wcsstr(text,L"第一个Creo"));
        path_for(path,first,L"chat-history.md");
        read_text(path,text);assert(!wcsstr(text,L"第二个Creo"));
        path_for(other,store_root,L"另一个独立配置");
        assert(store_init(other)&&!store_session_root[0]&&store_session_begin());
        assert(store_read(L"chat-history.md",text,_countof(text))&&!wcsstr(text,L"第二个Creo"));
        puts("SECOND PROCESS PASS: new session, first archive intact, profile isolation.");
    }else if(!wcscmp(argv[1],L"failure")){
        assert(store_write(L"recent-context.txt",L"不能丢失的旧聊天"));
        path_for(path,store_root,L"聊天记录");
        FILE *file=_wfopen(path,L"wb");
        assert(file&&fputs("blocked",file)>=0&&!fclose(file));
        assert(!store_session_begin()&&!store_session_root[0]);
        assert(store_read(L"recent-context.txt",text,_countof(text))&&wcsstr(text,L"不能丢失"));
        puts("FAILURE PASS: archive failure preserves all previous context.");
    }else assert(0);
    return 0;
}
