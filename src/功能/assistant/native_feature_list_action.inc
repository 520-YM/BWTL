static void native_feature_list_action(void){
    wchar_t document[MAX_PATH],notepad[MAX_PATH],argument[MAX_PATH+2];
    UINT system_length=GetSystemDirectoryW(notepad,MAX_PATH);
    if(!catalog_write_document(document,_countof(document))){
        MessageBoxW(native_window,L"无法写入本次会话的功能清单，请检查 BWTL 数据目录。",L"BWTL",MB_OK|MB_ICONWARNING);
        return;
    }
    if(!system_length||system_length>=MAX_PATH||
        wcscat_s(notepad,MAX_PATH,L"\\notepad.exe")||
        swprintf_s(argument,MAX_PATH+2,L"\"%ls\"",document)<0||
        (INT_PTR)ShellExecuteW(native_window,L"open",notepad,argument,NULL,SW_SHOWNORMAL)<=32){
        MessageBoxW(native_window,document,L"无法启动记事本，可手动打开此文件",MB_OK|MB_ICONWARNING);
    }
}
