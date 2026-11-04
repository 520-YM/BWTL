static void store_path(wchar_t path[MAX_PATH], const wchar_t *name) {
    swprintf_s(path, MAX_PATH, L"%ls\\%ls", store_root, name);
}
