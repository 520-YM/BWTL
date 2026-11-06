static void native_feature_list_action(void){
    wchar_t document[MAX_PATH],notepad[MAX_PATH],argument[MAX_PATH+2];
    DWORD attributes;
    UINT system_length=GetSystemDirectoryW(notepad,MAX_PATH);
    if(swprintf_s(document,MAX_PATH,L"%ls\\docs\\BWTL 功能列表.txt",runtime_root)<0){
        MessageBoxW(native_window,L"功能列表的路径过长。",L"BWTL",MB_OK|MB_ICONWARNING);
        return;
    }
    attributes=GetFileAttributesW(document);
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY)){
        MessageBoxW(native_window,L"当前版本缺少功能列表文件，请检查安装。",L"BWTL",MB_OK|MB_ICONWARNING);
        return;
    }
    if(!system_length||system_length>=MAX_PATH||
        wcscat_s(notepad,MAX_PATH,L"\\notepad.exe")||
        swprintf_s(argument,MAX_PATH+2,L"\"%ls\"",document)<0||
        (INT_PTR)ShellExecuteW(native_window,L"open",notepad,argument,NULL,SW_SHOWNORMAL)<=32){
        MessageBoxW(native_window,document,L"无法启动记事本，可手动打开此文件",MB_OK|MB_ICONWARNING);
    }
}
