/* Local UTF-8 persistence. No network, desktop or Creo dependencies. */
#ifndef CREO_CHAT_STORE_H
#define CREO_CHAT_STORE_H
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <windows.h>

static wchar_t store_root[MAX_PATH];
static void store_path(wchar_t path[MAX_PATH], const wchar_t *name) {
    swprintf_s(path, MAX_PATH, L"%ls\\%ls", store_root, name);
}
static int store_init(const wchar_t *root) {
    wcscpy_s(store_root, MAX_PATH, root);
    return CreateDirectoryW(root, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}
/* Keep the newest portion in memory; full history is separately append-only. */
static void store_tail_add(wchar_t *buffer, size_t capacity, const wchar_t *text) {
    size_t length = wcslen(buffer), count = wcslen(text), drop;
    if (count >= capacity) { text += count - capacity + 1; count = capacity - 1; }
    if (length + count >= capacity) {
        drop = length + count - capacity + 1;
        memmove(buffer, buffer + drop, (length - drop + 1) * sizeof(wchar_t));
    }
    wcscat_s(buffer, capacity, text);
}
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
/* Each memory is a complete line. Remove only an exact entry. */
static int store_forget(wchar_t *memory, const wchar_t *entry) {
    wchar_t *start = memory;
    size_t length = wcslen(entry);
    while (*start) {
        wchar_t *end = wcschr(start, L'\n');
        size_t count = end ? (size_t)(end - start) : wcslen(start);
        if (count == length && wcsncmp(start, entry, length) == 0) {
            wchar_t *next = end ? end + 1 : start + count;
            memmove(start, next, (wcslen(next) + 1) * sizeof(wchar_t));
            return 1;
        }
        if (!end) break;
        start = end + 1;
    }
    return 0;
}
#endif
