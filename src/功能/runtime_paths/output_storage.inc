static wchar_t storage_output_drive(void){
    wchar_t drive[2];
    DWORD length=GetEnvironmentVariableW(L"BWTL_OUTPUT_DRIVE",drive,_countof(drive));
    if(!length&&GetLastError()==ERROR_ENVVAR_NOT_FOUND){
        return runtime_data[1]==L':'?towupper(runtime_data[0]):0;
    }
    wchar_t letter=length==1?towupper(drive[0]):0;
    return letter>=L'A'&&letter<=L'Z'?letter:L'?';
}

static int storage_directory_on_drive(const wchar_t *directory,wchar_t drive){
    if(drive<L'A'||drive>L'Z'||!directory||towupper(directory[0])!=drive||
       directory[1]!=L':'||directory[2]!=L'\\')return 0;
    const wchar_t root[]={drive,L':',L'\\',0};
    if(GetDriveTypeW(root)!=DRIVE_FIXED)return 0;
    const DWORD attributes=GetFileAttributesW(directory);
    if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY))return 0;
    HANDLE handle=CreateFileW(directory,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(handle==INVALID_HANDLE_VALUE)return 0;
    wchar_t resolved[PRO_PATH_SIZE];
    DWORD length=GetFinalPathNameByHandleW(handle,resolved,_countof(resolved),VOLUME_NAME_DOS);
    CloseHandle(handle);
    return length>=7&&length<_countof(resolved)&&!wcsncmp(resolved,L"\\\\?\\",4)&&
        towupper(resolved[4])==drive&&resolved[5]==L':'&&resolved[6]==L'\\';
}

static int storage_output_directory(const wchar_t *directory){
    return storage_directory_on_drive(directory,storage_output_drive());
}

static int storage_prepare_output(void){
    ProPath directory;
    return !ProDirectoryCurrentGet(directory)&&storage_output_directory(directory)&&
        !ProConfigoptSet(L"override_store_back",L"yes");
}

static const wchar_t storage_output_message[]=L"BWTL 的文件写入安装时选择的数据盘。请在「工作目录」中选择该盘上的目录，再执行操作。";
