static int runtime_init(void){
    HMODULE module=NULL;wchar_t *slash,configured[MAX_PATH];
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&runtime_init,&module))return 0;
    if(!GetModuleFileNameW(module,runtime_root,MAX_PATH))return 0;
    slash=wcsrchr(runtime_root,L'\\');if(!slash)return 0;*slash=0;
    slash=wcsrchr(runtime_root,L'\\');
    if(slash&&!_wcsicmp(slash+1,L"bin"))*slash=0;
    if(swprintf_s(runtime_settings,MAX_PATH,L"%ls\\settings.ini",runtime_root)<0)return 0;
    GetPrivateProfileStringW(L"Runtime",L"DataDirectory",L"",configured,MAX_PATH,runtime_settings);
    if(configured[0]){if(configured[1]!=L':'&&wcsncmp(configured,L"\\\\",2))return 0;wcscpy_s(runtime_data,MAX_PATH,configured);}
    else if(swprintf_s(runtime_data,MAX_PATH,L"%ls\\data",runtime_root)<0)return 0;
    return CreateDirectoryW(runtime_data,NULL)||GetLastError()==ERROR_ALREADY_EXISTS;
}
