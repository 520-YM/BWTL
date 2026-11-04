static void store_tail_add(wchar_t *buffer, size_t capacity, const wchar_t *text) {
    size_t length = wcslen(buffer), count = wcslen(text), drop;
    if (count >= capacity) { text += count - capacity + 1; count = capacity - 1; }
    if (length + count >= capacity) {
        drop = length + count - capacity + 1;
        memmove(buffer, buffer + drop, (length - drop + 1) * sizeof(wchar_t));
    }
    wcscat_s(buffer, capacity, text);
}
