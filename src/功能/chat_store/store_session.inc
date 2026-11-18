/* Session files live with their attachments. The older cumulative files
   stay available in the data root and receive a one-time archive copy. */
static int store_archive_directory(const wchar_t *path){
    if(!CreateDirectoryW(path,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    DWORD attributes=GetFileAttributesW(path);
    return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&
        !(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}

static int store_archive_text(const wchar_t *path,const wchar_t *text){
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){
        if(GetLastError()!=ERROR_FILE_EXISTS)return 0;
        DWORD attributes=GetFileAttributesW(path);
        return attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY);
    }
    int capacity=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,NULL,0,NULL,NULL);
    char *bytes=capacity?malloc((size_t)capacity):NULL;
    DWORD written=0;
    int ok=bytes&&WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,bytes,capacity,NULL,NULL)==capacity&&
        WriteFile(file,bytes,(DWORD)capacity-1,&written,NULL)&&written==(DWORD)capacity-1&&FlushFileBuffers(file);
    free(bytes);
    CloseHandle(file);
    if(!ok)DeleteFileW(path);
    return ok;
}

static int store_archive_legacy(void){
    static const wchar_t *const names[]={
        L"chat-history.md",L"recent-context.txt",L"last-native-details.txt",L"imported-owner-history.md"
    };
    wchar_t folder[MAX_PATH],complete[MAX_PATH],source[MAX_PATH],target[MAX_PATH],temp[MAX_PATH];
    if(swprintf_s(folder,MAX_PATH,L"%ls\\历史记录_升级前",store_archive_root)<0||
       swprintf_s(complete,MAX_PATH,L"%ls\\归档完成.txt",folder)<0)return 0;
    DWORD attributes=GetFileAttributesW(complete);
    if(attributes!=INVALID_FILE_ATTRIBUTES)return !(attributes&FILE_ATTRIBUTE_DIRECTORY);
    if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)return 0;
    if(!store_archive_directory(folder))return 0;
    for(int i=0;i<_countof(names);i++){
        if(swprintf_s(source,MAX_PATH,L"%ls\\%ls",store_root,names[i])<0||
           swprintf_s(target,MAX_PATH,L"%ls\\%ls",folder,names[i])<0||
           swprintf_s(temp,MAX_PATH,L"%ls.copying",target)<0)return 0;
        attributes=GetFileAttributesW(source);
        if(attributes==INVALID_FILE_ATTRIBUTES){
            if(GetLastError()==ERROR_FILE_NOT_FOUND)continue;
            return 0;
        }
        if((attributes&FILE_ATTRIBUTE_DIRECTORY)||!CopyFileW(source,temp,FALSE)||
           !MoveFileExW(temp,target,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
            DeleteFileW(temp);
            return 0;
        }
    }
    wchar_t note[MAX_PATH+180];
    if(swprintf_s(note,_countof(note),L"旧版聊天数据已完整复制。原始附件保存在原数据目录的 attachments 文件夹。\r\n原数据目录\r\n%ls\r\n",store_root)<0)
        return 0;
    return store_archive_text(complete,note);
}

static int store_session_begin(void){
    if(store_session_root[0])return 1;
    wchar_t archive[MAX_PATH],session[MAX_PATH],path[MAX_PATH],header[MAX_PATH+250];
    FILETIME creation,exit,kernel,user,local;
    SYSTEMTIME started;
    if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user)||
       !FileTimeToLocalFileTime(&creation,&local)||!FileTimeToSystemTime(&local,&started))return 0;
    if(swprintf_s(archive,MAX_PATH,L"%ls\\聊天记录",store_root)<0||
       swprintf_s(session,MAX_PATH,L"%ls\\%04u-%02u-%02u_%02u-%02u-%02u-%03u_PID%lu_%08lx%08lx",archive,
            started.wYear,started.wMonth,started.wDay,started.wHour,started.wMinute,started.wSecond,
            started.wMilliseconds,GetCurrentProcessId(),creation.dwHighDateTime,creation.dwLowDateTime)<0||
       wcslen(session)+wcslen(L"\\last-native-details.txt.4294967295.tmp")>=MAX_PATH)return 0;
    if(!store_archive_directory(archive))return 0;
    wcscpy_s(store_archive_root,MAX_PATH,archive);
    if(!store_archive_legacy()||!store_archive_directory(session))return 0;
    if(swprintf_s(header,_countof(header),L"# BWTL 聊天记录\r\n\r\nCreo 进程 %lu\r\n\r\n开始时间 %04u-%02u-%02u %02u:%02u:%02u\r\n\r\n数据目录 %ls\r\n",
        GetCurrentProcessId(),started.wYear,started.wMonth,started.wDay,started.wHour,started.wMinute,started.wSecond,store_root)<0||
       swprintf_s(path,MAX_PATH,L"%ls\\chat-history.md",session)<0||!store_archive_text(path,header))return 0;
    wcscpy_s(store_session_root,MAX_PATH,session);
    return 1;
}

static FILE *store_history_open(void){
    wchar_t path[MAX_PATH];
    store_path(path,L"chat-history.md");
    return _wfopen(path,L"rt, ccs=UTF-8");
}
