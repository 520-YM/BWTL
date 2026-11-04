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
