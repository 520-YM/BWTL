static int memory_command(const wchar_t *value) {
    wchar_t candidate[12000];
    const wchar_t *entry = NULL;
    int forget = 0;
    if (wcscmp(value, L"查看记忆") == 0) { append(L"已保存的长期记忆："); append(memory[0] ? memory : L"暂无。输入“记住：内容”保存。"); return 1; }
    if (wcscmp(value, L"查看历史") == 0) { append(L"完整聊天保存在 settings.ini 指定的数据目录（默认插件目录 data）中的 chat-history.md；下方显示最近对话："); append(conversation[0] ? conversation : L"暂无。"); return 1; }
    if (wcscmp(value, L"新对话") == 0) {
        if (ai_process) { append(L"请等待 AI 回答完成，再开始新对话。"); return 1; }
        if (!store_ready || !store_write(L"recent-context.txt", L"")) { append(L"新对话创建失败，旧上下文已保留。"); return 1; }
        conversation[0] = 0; history[0] = 0;
        foundation_reset();memset(&pending_part,0,sizeof(pending_part));part_snapshot_clear(&request_snapshot);
        pending_plan.ready = 0;
        save_pending_plan();
        store_log(L"系统", L"开始新对话（长期记忆保留，历史未删除）。");
        append(L"已开始新对话，长期记忆保留，历史文件未删除。"); return 1;
    }
    if (wcsncmp(value, L"记住：", 3) == 0 || wcsncmp(value, L"记住:", 3) == 0) entry = value + 3;
    if (wcsncmp(value, L"忘记：", 3) == 0 || wcsncmp(value, L"忘记:", 3) == 0) { entry = value + 3; forget = 1; }
    if (!entry) return 0;
    if (ai_process) { append(L"请等待当前回答结束，再修改长期记忆。"); return 1; }
    while (iswspace(*entry)) entry++;
    if (!*entry || wcschr(entry, L'\n') || wcschr(entry, L'\r')) { append(L"每次请提供一条非空、单行的记忆。"); return 1; }
    wcscpy_s(candidate, 12000, memory);
    if (forget) {
        if (!store_forget(candidate, entry)) { append(L"未找到完全一致的记忆，请先输入“查看记忆”。"); return 1; }
    } else {
        /* A duplicate is already learned; do not add it again. */
        if (store_forget(candidate, entry)) { append(L"这条内容已经记住。"); return 1; }
        wcscpy_s(candidate, 12000, memory);
        if (wcslen(candidate) + wcslen(entry) + 2 >= 12000) { append(L"长期记忆已满，请先忘记不需要的条目。"); return 1; }
        wcscat_s(candidate, 12000, entry); wcscat_s(candidate, 12000, L"\n");
    }
    if (!store_ready || !store_write(L"memory.txt", candidate)) { append(L"保存记忆失败，原记忆未改变。"); return 1; }
    wcscpy_s(memory, 12000, candidate);
    /* Memory commands are not added to recent chat, so forgetting really
       removes the rule from the next prompt rather than quoting it again. */
    store_log(L"记忆管理", forget ? L"已删除一条长期记忆。" : L"已添加一条长期记忆。");
    append(forget ? L"已忘记这条长期记忆。旧聊天文件不删除；若旧对话仍提到它，可输入“新对话”清除近期上下文。" : L"已记住，下次启动 Creo 仍可使用。修改规则时请先“忘记：旧内容”，再“记住：新内容”。");
    return 1;
}
