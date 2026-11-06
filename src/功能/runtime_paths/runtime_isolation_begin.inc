static int runtime_isolation_begin(void){
    wchar_t name[96],profile[MAX_PATH];unsigned long hash=2166136261u;const wchar_t *p;FILE *f;DWORD length;
    length=GetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",profile,MAX_PATH);if(!length||length>=MAX_PATH)return 0;
    for(p=runtime_data;*p;p++){hash^=(unsigned long)towlower(*p);hash*=16777619u;}
    swprintf_s(name,96,L"Local\\BWTL_Data_%08lx",hash);
    runtime_data_mutex=CreateMutexW(NULL,TRUE,name);
    if(!runtime_data_mutex)return 0;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(runtime_data_mutex);runtime_data_mutex=NULL;return 0;}
    f=runtime_open(L"isolation-session.txt",L"wt, ccs=UTF-8");
    if(!f){CloseHandle(runtime_data_mutex);runtime_data_mutex=NULL;return 0;}
    fwprintf(f,L"PID=%lu\nLegacyBridgeLoaded=0\nProfile=%ls\nProgram=%ls\nData=%ls\n",GetCurrentProcessId(),profile,runtime_root,runtime_data);fclose(f);
    return 1;
}
