static int native_company_folder(const wchar_t *path){
    DWORD attributes;
    if(CreateDirectoryW(path,NULL))return 1;
    if(GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

static void native_company_action(void){
    wchar_t parent[MAX_PATH],root[MAX_PATH],standards[MAX_PATH],technical[MAX_PATH],*leaf;
    wcscpy_s(parent,MAX_PATH,runtime_data);
    leaf=wcsrchr(parent,L'\\');
    if(leaf&&!_wcsicmp(leaf+1,L"data"))*leaf=0;
    if(swprintf_s(root,MAX_PATH,L"%ls\\公司资料",parent)<0||
        swprintf_s(standards,MAX_PATH,L"%ls\\公司标准",root)<0||
        swprintf_s(technical,MAX_PATH,L"%ls\\技术文件",root)<0||
        !native_company_folder(root)||!native_company_folder(standards)||!native_company_folder(technical)){
        MessageBoxW(native_window,L"无法建立公司资料目录，请检查本机目录权限。",L"BWTL",MB_OK|MB_ICONWARNING);
        return;
    }
    if((INT_PTR)ShellExecuteW(native_window,L"open",root,NULL,NULL,SW_SHOWNORMAL)<=32)
        MessageBoxW(native_window,root,L"请在文件资源管理器中打开公司资料目录",MB_OK|MB_ICONINFORMATION);
}
