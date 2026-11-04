static int store_init(const wchar_t *root) {
    wcscpy_s(store_root, MAX_PATH, root);
    return CreateDirectoryW(root, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}
