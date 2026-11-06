static int runtime_isolation_verify(void){
    wchar_t enabled[8]={0},path[MAX_PATH];HMODULE modules[2048];DWORD bytes=0,i,length;
    if(GetEnvironmentVariableW(L"BWTL_ISOLATED_SESSION",enabled,8)!=1||wcscmp(enabled,L"1"))return 0;
    if(!EnumProcessModules(GetCurrentProcess(),modules,sizeof(modules),&bytes)||bytes>sizeof(modules))return 0;
    for(i=0;i<bytes/sizeof(HMODULE);i++){
        length=GetModuleFileNameW(modules[i],path,MAX_PATH);if(!length||length>=MAX_PATH)return 0;
        CharLowerBuffW(path,(DWORD)wcslen(path));
        if(wcsstr(path,L"creo_safe_resident"))return 0;
    }
    return 1;
}

