static int store_read(const wchar_t *name, wchar_t *buffer, size_t capacity) {
    wchar_t path[MAX_PATH], chunk[1025];
    FILE *file;
    size_t count;
    wint_t ch;
    buffer[0] = 0;
    store_path(path, name);
    file = _wfopen(path, L"rt, ccs=UTF-8");
    if (!file) return GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    do {
        count = 0;
        while (count < 1024 && (ch = fgetwc(file)) != WEOF) chunk[count++] = (wchar_t)ch;
        chunk[count] = 0;
        store_tail_add(buffer, capacity, chunk);
    } while (count == 1024);
    count = ferror(file);
    fclose(file);
    return !count;
}
