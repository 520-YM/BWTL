/* Copy the company's project template; the caller changes the Creo directory only on success. */
static const wchar_t project_template_leaf[]=L"（数量）（项目名称）（国家）（订单号）";
static const wchar_t project_model_subdir[]=L"V1.0\\1. 3D模型";

static int project_copy_path(wchar_t result[MAX_PATH],const wchar_t *parent,const wchar_t *leaf){
    return _snwprintf_s(result,MAX_PATH,_TRUNCATE,L"%ls\\%ls",parent,leaf)>=0;
}

static DWORD project_copy_check_directory(const wchar_t *path){
    DWORD attributes=GetFileAttributesW(path);
    if(attributes==INVALID_FILE_ATTRIBUTES)return GetLastError();
    if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)return ERROR_NOT_SUPPORTED;
    return attributes&FILE_ATTRIBUTE_DIRECTORY?ERROR_SUCCESS:ERROR_DIRECTORY;
}

static DWORD project_copy_tree(const wchar_t *source,const wchar_t *target,const wchar_t *destination){
    wchar_t search[MAX_PATH],from[MAX_PATH],to[MAX_PATH],published[MAX_PATH];
    DWORD error=project_copy_check_directory(source);
    if(error)return error;
    if(!project_copy_path(search,source,L"*"))return ERROR_FILENAME_EXCED_RANGE;
    WIN32_FIND_DATAW item;
    HANDLE find=FindFirstFileW(search,&item);
    if(find==INVALID_HANDLE_VALUE){
        error=GetLastError();
        return error==ERROR_FILE_NOT_FOUND?ERROR_SUCCESS:error;
    }
    do{
        if(!wcscmp(item.cFileName,L".")||!wcscmp(item.cFileName,L".."))continue;
        if(item.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT){error=ERROR_NOT_SUPPORTED;break;}
        if(!project_copy_path(from,source,item.cFileName)||!project_copy_path(to,target,item.cFileName)||
           !project_copy_path(published,destination,item.cFileName)){
            error=ERROR_FILENAME_EXCED_RANGE;
            break;
        }
        if(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
            if(!CreateDirectoryW(to,NULL))error=GetLastError();
            else error=project_copy_tree(from,to,published);
        }else if(!CopyFileW(from,to,TRUE))error=GetLastError();
    }while(!error&&FindNextFileW(find,&item));
    if(!error&&GetLastError()!=ERROR_NO_MORE_FILES)error=GetLastError();
    FindClose(find);
    return error;
}

/* Only called on a newly created staging directory under the verified project root. */
static int project_copy_discard_stage(const wchar_t *stage){
    wchar_t search[MAX_PATH],child[MAX_PATH];
    DWORD attributes=GetFileAttributesW(stage);
    if(attributes==INVALID_FILE_ATTRIBUTES)return 0;
    if(attributes&FILE_ATTRIBUTE_REPARSE_POINT)return RemoveDirectoryW(stage)!=0;
    if(!project_copy_path(search,stage,L"*"))return 0;
    WIN32_FIND_DATAW item;
    HANDLE find=FindFirstFileW(search,&item);
    if(find==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_FILE_NOT_FOUND)return 0;
    int clean=1;
    if(find!=INVALID_HANDLE_VALUE){
        do{
            if(!wcscmp(item.cFileName,L".")||!wcscmp(item.cFileName,L".."))continue;
            if(!project_copy_path(child,stage,item.cFileName)){clean=0;continue;}
            if(item.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){
                if(!project_copy_discard_stage(child))clean=0;
            }else{
                if((item.dwFileAttributes&FILE_ATTRIBUTE_READONLY)&&
                   !(item.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))
                    SetFileAttributesW(child,FILE_ATTRIBUTE_NORMAL);
                if(!DeleteFileW(child))clean=0;
            }
        }while(FindNextFileW(find,&item));
        if(GetLastError()!=ERROR_NO_MORE_FILES)clean=0;
        FindClose(find);
    }
    return RemoveDirectoryW(stage)&&clean;
}

static int project_copy_reserved_name(const wchar_t *name){
    /* DOS devices remain reserved even when the name has an extension. */
    size_t stem=wcscspn(name,L".");
    while(stem&&name[stem-1]==L' ')--stem;
    if(stem==3)
        return !_wcsnicmp(name,L"CON",3)||!_wcsnicmp(name,L"PRN",3)||
               !_wcsnicmp(name,L"AUX",3)||!_wcsnicmp(name,L"NUL",3);
    if(stem==4&&(!_wcsnicmp(name,L"COM",3)||!_wcsnicmp(name,L"LPT",3)))
        return wcschr(L"123456789¹²³",name[3])!=NULL;
    return (stem==6&&!_wcsnicmp(name,L"CONIN$",6))||
           (stem==7&&!_wcsnicmp(name,L"CONOUT$",7));
}

static DWORD project_copy_create(const wchar_t *root,const wchar_t *name,
                                 wchar_t workdir[MAX_PATH],wchar_t error_text[512]){
    wchar_t base[MAX_PATH],source[MAX_PATH],destination[MAX_PATH],stage[MAX_PATH];
    wchar_t stage_workdir[MAX_PATH],source_version[MAX_PATH],source_workdir[MAX_PATH],target_workdir[MAX_PATH];
    wchar_t default_leaf[MAX_PATH];
    DWORD error;
    workdir[0]=0;
    error_text[0]=0;
    DWORD length=GetFullPathNameW(root,MAX_PATH,base,NULL);
    if(!length||length>=MAX_PATH||towupper(base[0])!=storage_data_drive()||base[1]!=L':'||base[2]!=L'\\'){
        wcscpy_s(error_text,512,L"项目根目录不可用，请核对安装时选择的数据盘。");
        return ERROR_PATH_NOT_FOUND;
    }
    error=project_copy_check_directory(base);
    if(error){
        _snwprintf_s(error_text,512,_TRUNCATE,L"项目根目录不可用。\n%ls",base);
        return error;
    }
    HANDLE base_handle=CreateFileW(base,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                                  NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(base_handle==INVALID_HANDLE_VALUE){
        error=GetLastError();
        _snwprintf_s(error_text,512,_TRUNCATE,L"无法打开项目根目录。\n%ls",base);
        return error;
    }
    wchar_t resolved[MAX_PATH+4];
    length=GetFinalPathNameByHandleW(base_handle,resolved,_countof(resolved),VOLUME_NAME_DOS);
    CloseHandle(base_handle);
    if(length<=4||length>=_countof(resolved)||wcsncmp(resolved,L"\\\\?\\",4)||_wcsicmp(resolved+4,base)){
        wcscpy_s(error_text,512,L"项目根目录必须是所选数据盘上的实际目录。");
        return ERROR_NOT_SUPPORTED;
    }

    const wchar_t *leaf=name?name:L"";
    const wchar_t *nonspace=leaf;
    while(iswspace(*nonspace))++nonspace;
    if(*nonspace){
        size_t count=wcslen(leaf);
        if(count>=MAX_PATH){
            wcscpy_s(error_text,512,L"项目文件夹名称太长，请缩短后再试。");
            return ERROR_FILENAME_EXCED_RANGE;
        }
        for(size_t i=0;i<count;++i){
            if(leaf[i]<32||wcschr(L"<>:\"/\\|?*",leaf[i])){
                wcscpy_s(error_text,512,L"项目文件夹名称中包含 Windows 文件夹名不能使用的字符。");
                return ERROR_INVALID_NAME;
            }
        }
        if(leaf[count-1]==L'.'||leaf[count-1]==L' '||project_copy_reserved_name(leaf)){
            wcscpy_s(error_text,512,L"项目文件夹名称不能以空格或句点结尾，也不能使用 Windows 保留名称。");
            return ERROR_INVALID_NAME;
        }
    }else{
        SYSTEMTIME now;
        GetLocalTime(&now);
        swprintf_s(default_leaf,_countof(default_leaf),
                   L"（数量）（项目名称%04u%02u%02u-%02u%02u%02u）（国家）（订单号）",
                   now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
        leaf=default_leaf;
    }
    if(!project_copy_path(source,base,project_template_leaf)||
       !project_copy_path(source_version,source,L"V1.0")||
       !project_copy_path(source_workdir,source,project_model_subdir)||
       !project_copy_path(destination,base,leaf)||
       !project_copy_path(target_workdir,destination,project_model_subdir)){
        wcscpy_s(error_text,512,L"项目路径太长，请缩短项目名称。");
        return ERROR_FILENAME_EXCED_RANGE;
    }
    error=project_copy_check_directory(source);
    if(error){
        _snwprintf_s(error_text,512,_TRUNCATE,L"项目模板目录不可用。\n%ls",source);
        return error;
    }
    error=project_copy_check_directory(source_version);
    if(!error)error=project_copy_check_directory(source_workdir);
    if(error){
        _snwprintf_s(error_text,512,_TRUNCATE,L"项目模板缺少可用的 V1.0\\1. 3D模型 文件夹。\n%ls",source_workdir);
        return error;
    }
    if(GetFileAttributesW(destination)!=INVALID_FILE_ATTRIBUTES){
        _snwprintf_s(error_text,512,_TRUNCATE,L"同名项目已经存在，请更换项目名称。\n%ls",destination);
        return ERROR_ALREADY_EXISTS;
    }

    const DWORD tick=GetTickCount();
    error=ERROR_ALREADY_EXISTS;
    for(unsigned attempt=0;attempt<16;++attempt){
        wchar_t stage_leaf[80];
        swprintf_s(stage_leaf,_countof(stage_leaf),L".bwtl-project-%08lx-%08lx-%u",GetCurrentProcessId(),tick,attempt);
        if(!project_copy_path(stage,base,stage_leaf)||!project_copy_path(stage_workdir,stage,project_model_subdir)){
            error=ERROR_FILENAME_EXCED_RANGE;
            break;
        }
        if(CreateDirectoryW(stage,NULL)){error=ERROR_SUCCESS;break;}
        error=GetLastError();
        if(error!=ERROR_ALREADY_EXISTS)break;
    }
    if(error){
        _snwprintf_s(error_text,512,_TRUNCATE,L"无法在项目根目录准备复制项目，Windows 错误 %lu。\n%ls",error,base);
        return error;
    }
    error=project_copy_tree(source,stage,destination);
    if(!error)error=project_copy_check_directory(stage_workdir);
    if(!error&&!MoveFileExW(stage,destination,MOVEFILE_WRITE_THROUGH))error=GetLastError();
    if(error){
        int cleaned=project_copy_discard_stage(stage);
        if(!cleaned)
            _snwprintf_s(error_text,512,_TRUNCATE,L"复制项目失败（Windows 错误 %lu），临时文件夹未完全清理。\n%ls",error,stage);
        else if(error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS)
            _snwprintf_s(error_text,512,_TRUNCATE,L"同名项目已经存在，请更换项目名称。\n%ls",destination);
        else if(error==ERROR_NOT_SUPPORTED)
            wcscpy_s(error_text,512,L"模板中含有链接或重解析点，无法将它复制为独立项目。");
        else if(error==ERROR_FILENAME_EXCED_RANGE)
            wcscpy_s(error_text,512,L"模板内的文件路径太长，请缩短项目名称或模板子目录。");
        else
            _snwprintf_s(error_text,512,_TRUNCATE,L"复制项目失败（Windows 错误 %lu）。\n%ls",error,destination);
        return error;
    }
    wcscpy_s(workdir,MAX_PATH,target_workdir);
    return ERROR_SUCCESS;
}