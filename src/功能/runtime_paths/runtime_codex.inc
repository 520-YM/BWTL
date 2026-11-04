static int runtime_codex(wchar_t exe[MAX_PATH]){
    wchar_t local[MAX_PATH],pattern[MAX_PATH],candidate[MAX_PATH];WIN32_FIND_DATAW data;HANDLE find;FILETIME newest={0};int found=0;
    GetPrivateProfileStringW(L"Runtime",L"CodexPath",L"",exe,MAX_PATH,runtime_settings);
    if(exe[0])return runtime_exe_exists(exe);
    if(SearchPathW(NULL,L"codex.exe",NULL,MAX_PATH,exe,NULL)&&runtime_exe_exists(exe))return 1;
    if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH))return 0;
    if(swprintf_s(pattern,MAX_PATH,L"%ls\\OpenAI\\Codex\\bin\\*",local)<0)return 0;
    find=FindFirstFileW(pattern,&data);if(find==INVALID_HANDLE_VALUE)return 0;
    do {
        if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||data.cFileName[0]==L'.')continue;
        if(swprintf_s(candidate,MAX_PATH,L"%ls\\OpenAI\\Codex\\bin\\%ls\\codex.exe",local,data.cFileName)<0)continue;
        if(runtime_exe_exists(candidate)&&(!found||CompareFileTime(&data.ftLastWriteTime,&newest)>0)){
            wcscpy_s(exe,MAX_PATH,candidate);newest=data.ftLastWriteTime;found=1;
        }
    }while(FindNextFileW(find,&data));FindClose(find);return found;
}
