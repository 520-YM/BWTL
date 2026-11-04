static void save_turn(const wchar_t *role, const wchar_t *text) {
    wchar_t prefix[100];
    swprintf_s(prefix, 100, L"\n%ls：", role);
    store_tail_add(conversation, 24000, prefix);
    store_tail_add(conversation, 24000, text);
    if (!store_ready || !store_log(role, text) || !store_write(L"recent-context.txt", conversation))
        append(L"注意：聊天记录保存失败，请检查 data 目录权限。当前内容仍在内存中。");
}
