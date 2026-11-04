static int store_log(const wchar_t *role, const wchar_t *value) {
    wchar_t path[MAX_PATH];
    SYSTEMTIME now;
    FILE *file;
    int ok;
    GetLocalTime(&now);
    store_path(path, L"chat-history.md");
    file = _wfopen(path, L"at, ccs=UTF-8");
    if (!file) return 0;
    ok = fwprintf(file, L"\n## %04u-%02u-%02u %02u:%02u:%02u · %ls\n\n%ls\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, role, value) >= 0;
    if (fclose(file)) ok = 0;
    return ok;
}
