#define _CRT_SECURE_NO_WARNINGS
#include "../src/chat_store.h"
#include <assert.h>

int wmain(int argc, wchar_t **argv) {
    wchar_t buffer[12000], tail_buffer[9] = L"1234";
    assert(argc == 3);
    assert(store_init(argv[2]));
    if (wcscmp(argv[1], L"write") == 0) {
        assert(store_write(L"memory.txt", L"用中文回答\n钣金默认使用毫米\n"));
        assert(store_write(L"recent-context.txt", L"用户：这是上次的聊天\n助手：下次继续\n"));
        assert(store_log(L"用户", L"中文历史：折弯与铆接"));
        assert(store_log(L"助手", L"历史记录应追加而不是覆盖"));
        wprintf(L"WRITE PASS\n");
    } else {
        assert(store_read(L"memory.txt", buffer, 12000));
        assert(wcscmp(buffer, L"用中文回答\n钣金默认使用毫米\n") == 0);
        assert(store_forget(buffer, L"钣金默认使用毫米"));
        assert(wcscmp(buffer, L"用中文回答\n") == 0);
        assert(!store_forget(buffer, L"中文"));
        assert(store_write(L"memory.txt", buffer));
        assert(store_read(L"recent-context.txt", buffer, 12000));
        assert(wcsstr(buffer, L"这是上次的聊天"));
        assert(store_read(L"chat-history.md", buffer, 12000));
        assert(wcsstr(buffer, L"折弯与铆接") && wcsstr(buffer, L"追加而不是覆盖"));
        store_tail_add(tail_buffer, 9, L"56789"); assert(wcscmp(tail_buffer, L"23456789") == 0);
        store_tail_add(tail_buffer, 9, L"ABCDEFGHIJK"); assert(wcscmp(tail_buffer, L"DEFGHIJK") == 0);
        assert(store_write(L"recent-context.txt", L""));
        assert(store_read(L"recent-context.txt", buffer, 12000) && !buffer[0]);
        assert(store_read(L"memory.txt", buffer, 12000) && wcscmp(buffer, L"用中文回答\n") == 0);
        assert(store_read(L"missing.txt", buffer, 12000) && !buffer[0]);
        assert(!store_write(L"nonexistent\\fail.txt", L"cannot write"));
        wprintf(L"RESTORE / FORGET / HISTORY / BOUNDS / NEW CHAT / FAILURE PASS\n");
    }
    return 0;
}
