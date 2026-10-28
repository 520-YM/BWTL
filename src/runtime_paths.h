#ifndef BWTL_RUNTIME_PATHS_H
#define BWTL_RUNTIME_PATHS_H
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
static wchar_t runtime_root[MAX_PATH],runtime_data[MAX_PATH],runtime_settings[MAX_PATH];
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
static FILE *runtime_open(const wchar_t *name,const wchar_t *mode){
    wchar_t path[MAX_PATH];if(!runtime_data[0])return NULL;
    if(swprintf_s(path,MAX_PATH,L"%ls\\%ls",runtime_data,name)<0)return NULL;return _wfopen(path,mode);
}
static int runtime_exe_exists(const wchar_t *path){
    DWORD attr=GetFileAttributesW(path);const wchar_t *ext=wcsrchr(path,L'.');
    return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&FILE_ATTRIBUTE_DIRECTORY)&&ext&&!_wcsicmp(ext,L".exe");
}
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
#endif
