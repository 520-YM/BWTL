#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <assert.h>
#include "../src/runtime_paths.h"
int wmain(int argc,wchar_t **argv){
    HMODULE fixture;wchar_t temp[MAX_PATH],data[MAX_PATH];
    assert(argc==2);
    SetEnvironmentVariableW(L"BWTL_ISOLATED_SESSION",NULL);assert(!runtime_isolation_verify());
    SetEnvironmentVariableW(L"BWTL_ISOLATED_SESSION",L"12345678901234567890");assert(!runtime_isolation_verify());
    SetEnvironmentVariableW(L"BWTL_ISOLATED_SESSION",L"1");assert(runtime_isolation_verify());
    fixture=LoadLibraryW(argv[1]);assert(fixture);assert(!runtime_isolation_verify());
    FreeLibrary(fixture);assert(runtime_isolation_verify());
    GetTempPathW(MAX_PATH,temp);swprintf_s(data,MAX_PATH,L"%lsBWTL-guard-test-%lu",temp,GetCurrentProcessId());
    assert(CreateDirectoryW(data,NULL));wcscpy_s(runtime_data,MAX_PATH,data);
    SetEnvironmentVariableW(L"BWTL_PROFILE_ROOT",data);assert(runtime_isolation_begin());runtime_isolation_end();
    puts("ISOLATION GUARD PASS: marker validation, loaded legacy-name rejection, clean-process acceptance and data-session log/mutex");return 0;
}
