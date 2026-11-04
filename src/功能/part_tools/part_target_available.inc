static int part_target_available(ProMdl model,const wchar_t *base,const wchar_t *extension){
    ProMdl existing=NULL;ProMdlName name;ProPath dir;wchar_t path[PRO_PATH_SIZE];WIN32_FIND_DATAW data;HANDLE find;
    wcscpy_s(name,PRO_NAME_SIZE,base);
    if(!_wcsicmp(extension,L"prt") && (!ProMdlnameInit(name,PRO_MDL_PART,&existing)||!ProMdlnameInit(name,PRO_MDL_ASSEMBLY,&existing)))return 0;
    if(ProDirectoryCurrentGet(dir))return 0;
    if(swprintf_s(path,PRO_PATH_SIZE,L"%ls\\%ls.%ls*",dir,base,extension)<0)return 0;
    find=FindFirstFileW(path,&data);if(find!=INVALID_HANDLE_VALUE){FindClose(find);return 0;}
    return GetLastError()==ERROR_FILE_NOT_FOUND;
}
