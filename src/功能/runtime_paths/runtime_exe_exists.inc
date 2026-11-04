static int runtime_exe_exists(const wchar_t *path){
    DWORD attr=GetFileAttributesW(path);const wchar_t *ext=wcsrchr(path,L'.');
    return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_DIRECTORY)&&ext&&!_wcsicmp(ext,L".exe");
}
