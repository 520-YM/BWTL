static void store_path(wchar_t path[MAX_PATH], const wchar_t *name) {
    int session=store_session_root[0]&&(!wcscmp(name,L"chat-history.md")||
        !wcscmp(name,L"recent-context.txt")||!wcscmp(name,L"last-native-details.txt"));
    swprintf_s(path, MAX_PATH, L"%ls\\%ls",session?store_session_root:store_root,name);
}
