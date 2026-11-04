static int store_write(const wchar_t *name, const wchar_t *value) {
    wchar_t path[MAX_PATH], temp[MAX_PATH];
    FILE *file;
    int ok;
    store_path(path, name);
    swprintf_s(temp, MAX_PATH, L"%ls.%lu.tmp", path, GetCurrentProcessId());
    file = _wfopen(temp, L"wt, ccs=UTF-8");
    if (!file) return 0;
    ok = fputws(value, file) >= 0;
    if (fclose(file)) ok = 0;
    if (ok) ok = MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) DeleteFileW(temp);
    return ok;
}
