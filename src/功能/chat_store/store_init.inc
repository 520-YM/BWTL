static int store_init(const wchar_t *root) {
    if(_wcsicmp(store_root,root)){store_session_root[0]=store_archive_root[0]=0;}
    wcscpy_s(store_root, MAX_PATH, root);
    return CreateDirectoryW(root, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}
